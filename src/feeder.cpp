// #define USE_SERIAL_DBG 1

// make vscode happy...
#ifndef __ESP__
#define __ESP__
#endif

// make vscode happy...
#undef __APPLE__

#include "platform.h"
#include "scheduler.h"

#include "net.h"
#include "ota.h"
#include "mqtt.h"
#include "hass.h"

#include "led.h"
#include "switch.h"
#include "motor_interval.h"
#include "airq_bme280.h"

// entities for core functionality
ustd::Scheduler sched( 10, 16, 32 );
ustd::Net       net( LED_BUILTIN );
ustd::Ota       ota;
ustd::Mqtt      mqtt;
ustd::Hass      hass( "Cat Feeder 1", "YeaSoft Int'l", "Balimo 4L WiFi", "1.0.0" );

// entities for connected hardware
ustd::Led               led( "led", D3, true );
ustd::Switch            feed( "feed", 10, ustd::Switch::Mode::Default, false );
ustd::Switch            user( "user", D7, ustd::Switch::Mode::Default, false );
ustd::MotorInterval     feeder( "feeder", D8, D5, 3000, true, true );
ustd::AirQualityBme280  env( "bme280", BME280_ADDRESS_ALTERNATE, ustd::AirQualityBme280::SampleMode::MEDIUM );

// other settings
unsigned long   btnTimeout = 1000;
unsigned long   lastPush = 0;
unsigned        intervals = 0;

// user button logic
void user_button_handler( String topic, String msg, String originator ) {
    if ( msg == "on" ) {
        ++intervals;
        lastPush = millis();
    }
}

// feed button logic
void feed_button_handler( String topic, String msg, String originator ) {
    if ( topic == "feed/switch/state" ) {
        if ( msg == "on" ) {
            feeder.start();
        } else if ( msg == "off" ) {
            feeder.stop();
        }
    }
}

// feeder activity led logic
void feeder_led_handler( String topic, String msg, String originator ) {
    led.setMode( ustd::Led::Mode::Passive );
    led.set( msg == "on" );
}

// feeder error logic
void feeder_error_handler( String topic, String msg, String originator ) {
    if ( msg != "OK" ) {
        led.setMode( ustd::Led::Mode::Blink, 200 );
    }
}

// main application command handler
void app_command_handler( String topic, String msg, String originator ) {
    if ( topic == "cmnd/restart" ) {
        ESP.restart();
    }
}

// main application task
void app() {
    if ( intervals && ustd::timeDiff( lastPush, millis() ) > btnTimeout ) {
        feeder.set( intervals );
        intervals = 0;
        lastPush = 0;
    }
}

// application setup
void setup() {
#ifdef USE_SERIAL_DBG
    Serial.begin( 115200 );
    Serial.println( "Startup" );
#endif // USE_SERIAL_DBG

    // initialize core entities
    net.begin( &sched );
    ota.begin( &sched );
    mqtt.begin( &sched, "", "", "" );

    // initialize hardware
    led.begin( &sched );
    feed.begin( &sched );
    user.begin( &sched );
    feeder.begin( &sched );
    env.begin( &sched );
    hass.begin( &sched );

    // configure hardware
    feeder.setMaxIntervals( 16 );
    feeder.setDefaultIntervals( 2 );

    // configure home assistant integration
    hass.addAttributes( "bme280", "Bosch Sensortec", "BME280", env.AIRQUALITY_VERSION );
    hass.addSensor( "bme280", "temperature", "Temperature", "\\u00B0C", "temperature", "", "", "bme280" );
    hass.addSensor( "bme280", "humidity", "Humidity", "%", "humidity", "", "", "bme280" );
    hass.addSensor( "bme280", "pressure", "Pressure", "hPa", "pressure", "", "", "bme280" );
    hass.addSwitch( "feeder", "device", "mdi:cat" );

    // initialize application
    int tID = sched.add( app, "main", 50000 );
    sched.subscribe( tID, "user/switch/state", user_button_handler );
    sched.subscribe( tID, "feed/switch/state", feed_button_handler );
    sched.subscribe( tID, "feeder/switch/state", feeder_led_handler );
    sched.subscribe( tID, "feeder/switch/result", feeder_error_handler );
    sched.subscribe( tID, "cmnd/#", app_command_handler );
}

// application runtime
void loop() {
    sched.loop();
}