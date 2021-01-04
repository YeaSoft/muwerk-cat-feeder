#pragma once

#ifdef __ESP__

#include "scheduler.h"
#include "jsonfiles.h"

namespace ustd {

    class Hass {
    protected:
        Scheduler* pSched;
        int tID;
        bool connected = false;
        bool autodiscovery;
        String rawMac;
        String hexMac;

        String ipAddress = "${IPADDRESS}";
        String hostName = "${HOSTNAME}";

        String pathPrefix;
        String lastWillTopic;
        String lastWillMessage;

        String deviceName;
        String deviceManufacturer;
        String deviceModel;
        String deviceVersion;

        ustd::array<String> attribGroups;
        ustd::array<String> attribValues;

        ustd::array<String> entityKeys;
        ustd::array<String> entityNames;
        ustd::array<String> entityConfigs;

        long rssiVal = -99;

    public:
        Hass( String name, String manufacturer = "muWerk", String model = "unknown", String version = "0.0.1" ) {
            deviceName = name;
            deviceManufacturer = manufacturer;
            deviceModel = model;
            deviceVersion = version;
        }

        ~Hass() {}

        void begin( Scheduler* _pSched, bool initialAutodiscovery = false ) {

            auto onMessage = [this]( String topic, String msg, String originator ) {
                this->onMessage( topic, msg, originator );
            };

            auto onCommand = [this]( String topic, String msg, String originator ) {
                this->onCommand( topic, msg, originator );
            };

            pSched = _pSched;
            autodiscovery = ustd::muReadVal( "hasstest/autodiscovery", initialAutodiscovery ? "true" : "false" ) == "true";
            updateMacAddr();
            addAttributes( "device" );

            // react to network state changes
            pSched->subscribe( tID, "mqtt/config", onMessage );
            pSched->subscribe( tID, "mqtt/state", onMessage );
            pSched->subscribe( tID, "net/network", onMessage );
            pSched->subscribe( tID, "net/rssi", onMessage );

            // react to commands
            pSched->subscribe( tID, "hass/cmnd/#", onCommand );

            // request current state
            pSched->publish( "mqtt/state/get" );
        }

        void setAutoDiscovery( bool enabled, bool persistent = true ) {
            if ( autodiscovery != enabled ) {
                autodiscovery = enabled;
                updateHA();
            }
            if ( persistent ) {
                ustd::muWriteVal( "hasstest/autodiscovery", autodiscovery ? "true" : "false" );
            }
        }

        void addAttributes( String attribGroup, String manufacturer = "", String model = "", String version = "" ) {
            for ( unsigned int i = 0; i < attribGroups.length(); i++ ) {
                if ( attribGroups[ i ] == attribGroup ) {
                    // insert only unique attribute groups
                    return;
                }
            }
            attribGroups.add( attribGroup );
            attribValues.add(
                makeProperty( "Manufacturer", manufacturer.length() ? manufacturer : deviceManufacturer ) +
                makeProperty( "Model", model.length() ? model : deviceModel ) +
                makeProperty( "Version", version.length() ? version : deviceVersion, true )
            );
        }

        void addSensor( String entityName, String entityValue, String friendlyName, String unit_of_meas = "", String dev_cla = "", String icon = "", String val_tpl = "", String attrib_group = "", int exp_aft = -1, bool frc_upd = false ) {
            entityNames.add( entityName + " " + friendlyName );

            String entityKey = hexMac + "_" + entityName + "_" + friendlyName;
            entityKey.replace( " ", "_" );
            entityKeys.add( "sensor/" + entityKey );

            String entityTopic = entityName;
            entityTopic.replace( " ", "_" );

            String json_attr_t = attrib_group.length() ? "~hass/attribs/" + attrib_group : "";

            entityConfigs.add(
                makeProperty( "stat_t", "~" + entityTopic + "/sensor/" + entityValue ) +
                makeConditionalProperty( "json_attr_t", json_attr_t ) +
                makeConditionalProperty( "val_tpl", val_tpl ) +
                makeConditionalProperty( "dev_cla", dev_cla ) +
                makeConditionalProperty( "unit_of_meas", unit_of_meas ) +
                makeConditionalProperty( "exp_aft", exp_aft, -1 ) +
                makeConditionalProperty( "frc_upd", frc_upd, false ) +
                makeConditionalProperty( "ic", icon ) +
                makeProperty( "uniq_id", entityKey )
            );
        }

        void addLight( String entityName, String attrib_group = "" ) {
            entityNames.add( entityName );

            String entityKey = hexMac + "_" + entityName;
            entityKey.replace( " ", "_" );
            entityKeys.add( "light/" + entityKey );

            String entityTopic = entityName;
            entityTopic.replace( " ", "_" );

            String json_attr_t = attrib_group.length() ? "~hass/attribs/" + attrib_group : "";

            entityConfigs.add(
                makeProperty( "stat_t", "~" + entityTopic + "/light/state" ) +
                makeProperty( "cmd_t", hostName + "/" + entityTopic + "/light/set" ) +
                makeConditionalProperty( "json_attr_t", json_attr_t ) +
                makeProperty( "bri_stat_t", "~" + entityTopic + "/light/unitbrightness" ) +
                makeProperty( "bri_scl", "100" ) +
                makeProperty( "bri_val_tpl", "{{ value | float * 100 | round(0) }}" ) +
                makeProperty( "bri_cmd_t", hostName + "/" + entityTopic + "/light/set" ) +
                makeProperty( "on_cmd_type", "brightness" ) +
                makeProperty( "pl_on", "on" ) +
                makeProperty( "pl_off", "off" ) +
                makeProperty( "uniq_id", entityKey )
            );
        }

        void addSwitch( String entityName, String attrib_group = "", String icon = "" ) {
            entityNames.add( entityName );

            String entityKey = hexMac + "_" + entityName;
            entityKey.replace( " ", "_" );
            entityKeys.add( "switch/" + entityKey );

            String entityTopic = entityName;
            entityTopic.replace( " ", "_" );

            String json_attr_t = attrib_group.length() ? "~hass/attribs/" + attrib_group : "";

            entityConfigs.add(
                makeProperty( "stat_t", "~" + entityTopic + "/switch/state" ) +
                makeProperty( "stat_on", "on" ) +
                makeProperty( "stat_off", "off" ) +
                makeProperty( "cmd_t", hostName + "/" + entityTopic + "/switch/set" ) +
                makeConditionalProperty( "json_attr_t", json_attr_t ) +
                makeConditionalProperty( "ic", icon ) +
                makeProperty( "pl_on", "on" ) +
                makeProperty( "pl_off", "off" ) +
                makeProperty( "uniq_id", entityKey )
            );
        }

    protected:
        String makeDevice() {
            return "{" +
                makeProperty( "ids", "[ " + makeString( rawMac, true ) + "]", false, true ) +
                makeProperty( "cns", "[[" +
                    makeString( "IP" ) +
                    makeString( ipAddress, true ) +
                    "],[" +
                    makeString( "Host" ) +
                    makeString( hostName, true ) +
                    "]]", false, true
                ) +
                makeProperty( "name", deviceName ) +
                makeProperty( "mf", deviceManufacturer ) +
                makeProperty( "mdl", deviceModel ) +
                makeProperty( "sw", deviceVersion, true ) + "}";
        }

        void updateMacAddr() {
            if ( rawMac == "" ) {
                rawMac = WiFi.macAddress().c_str();
                hexMac = rawMac;
                hexMac.replace( ":", "" );
            }
        }

        void updatePlaceholders() {
            for ( unsigned int i = 0; i < entityConfigs.length(); i++ ) {
                entityConfigs[ i ].replace( "${HOSTNAME}", hostName );
                entityConfigs[ i ].replace( "${IPADDRESS}", ipAddress );
            }
        }

        void publishAttrib( String attrTopic, String attribs = "" ) {
            updateMacAddr();

            if ( attribs.length() == 0 ) {
                attribs =
                    makeProperty( "Manufacturer", deviceManufacturer ) +
                    makeProperty( "Model", deviceModel ) +
                    makeProperty( "Version", deviceVersion, true );
            }

            pSched->publish( attrTopic, "{" +
                makeProperty( "RSSI", String( WifiGetRssiAsQuality( rssiVal ) ) ) +
                makeProperty( "Signal (dBm)", String( rssiVal ) ) +
                makeProperty( "Mac", rawMac ) +
                makeProperty( "IP", ipAddress ) +
                makeProperty( "Host", hostName ) +
                attribs + "}"
            );
        }

        void publishConfig( String configTopic, String entityName, String entityConfig ) {
            pSched->publish( configTopic, "{" +
                makeProperty( "~", pathPrefix + "/" ) +
                makeProperty( "name", hostName + " " + entityName ) +
                makeProperty( "avty_t", "~mqtt/state" ) +
                makeProperty( "pl_avail", "connected" ) +
                makeProperty( "pl_not_avail", lastWillMessage ) +
                entityConfig +
                makeProperty( "dev", makeDevice(), true, true ) + "}"
            );
        }

        void publishAttribs() {
            for ( unsigned int i = 0; i < attribGroups.length(); i++ ) {
                publishAttrib( "hass/attribs/" + attribGroups[ i ], attribValues[ i ] );
            }
        }

        void unpublishAttribs() {
            for ( unsigned int i = 0; i < attribGroups.length(); i++ ) {
                pSched->publish( "hass/attribs/" + attribGroups[ i ], "" );
            }
        }

        void publishConfigs() {
            // device config
            pSched->publish( "!homeassistant/sensor/" + hexMac + "_status/config", "{" +
                makeProperty( "~", pathPrefix + "/" ) +
                makeProperty( "name", hostName + " Status" ) +
                makeProperty( "stat_t", "~hass/attribs/device" ) +
                makeProperty( "avty_t", "~mqtt/state" ) +
                makeProperty( "pl_avail", "connected" ) +
                makeProperty( "pl_not_avail", "disconnected" ) +
                makeProperty( "json_attr_t", "~hass/attribs/device" ) +
                makeProperty( "unit_of_meas", "%" ) +
                makeProperty( "val_tpl", "{{value_json['RSSI']}}" ) +
                makeProperty( "ic", "mdi:information-outline" ) +
                makeProperty( "uniq_id", hexMac + "_status" ) +
                makeProperty( "dev", makeDevice(), true, true ) + "}"
            );
            // entity configs
            for ( unsigned int i = 0; i < entityKeys.length(); i++ ) {
                publishConfig( "!homeassistant/" + entityKeys[ i ] + "/config", entityNames[ i ], entityConfigs[ i ] );
            }
        }

        void unpublishConfigs() {
            // device config
            pSched->publish( "!homeassistant/sensor/" + hexMac + "_status/config", "" );
            // entity configs
            for ( unsigned int i = 0; i < entityKeys.length(); i++ ) {
                pSched->publish( "!homeassistant/" + entityKeys[ i ] + "/config", "" );
            }
        }

        void onCommand( String topic, String msg, String originator ) {
            if ( topic == "hass/cmnd/enable" ) {
                bool persistent = msg == "save" || msg == "on" || msg == "1";
                setAutoDiscovery( true, persistent );
            } else if ( topic == "hass/cmnd/disable" ) {
                bool persistent = msg == "save" || msg == "on" || msg == "1";
                setAutoDiscovery( false, persistent );
            }
        }

        void onMessage( String topic, String msg, String originator ) {
            if ( topic == "net/rssi" ) {
                JSONVar mqttJsonMsg = JSON.parse( msg );
                if ( JSON.typeof( mqttJsonMsg ) == "undefined" ) {
                    return;
                }
                rssiVal = (long)mqttJsonMsg[ "rssi" ];
                if ( autodiscovery ) {
                    publishAttribs();
                }
            } else if ( topic == "net/network" ) {
                JSONVar mqttJsonMsg = JSON.parse( msg );
                if ( JSON.typeof( mqttJsonMsg ) == "undefined" ) {
                    return;
                }
                if ( !strcmp( (const char*)mqttJsonMsg[ "state" ], "connected" ) ) {
                    ipAddress = (const char*)mqttJsonMsg[ "ip" ];
                    hostName = (const char*)mqttJsonMsg[ "hostname" ];
                    updatePlaceholders();
                }
            } else if ( topic == "mqtt/config" ) {
                char szMessage[ 180 ];
                strncpy( szMessage, msg.c_str(), sizeof( szMessage ) / sizeof( char ) );
                szMessage[ ( sizeof( szMessage ) / sizeof( char ) ) - 1 ] = 0;

                char* lwt = strchr( szMessage, '+' );
                if ( lwt ) {
                    *lwt = 0;
                    ++lwt;
                }
                char* lwm = strchr( lwt, '+' );
                if ( lwm ) {
                    *lwm = 0;
                    ++lwm;
                }

                pathPrefix = szMessage;
                lastWillTopic = lwt;
                lastWillMessage = lwm;
            } else if ( topic == "mqtt/state" ) {
                bool previous = connected;
                connected = msg == "connected";
                if ( connected != previous ) {
                    updateHA();
                }
            }
        }

        void updateHA() {
            if ( connected ) {
                if ( autodiscovery ) {
                    publishAttribs();
                    publishConfigs();
                } else {
                    unpublishConfigs();
                    unpublishAttribs();
                }
            }
        }

        static String makeString( String value, bool last = false ) {
            return  "\"" + value + ( last ? "\"" : "\"," );
        }

        static String makeProperty( String key, String value, bool last = false, bool raw = false ) {
            const char* sep = raw ? "\":" : "\":\"";
            const char* end = raw ? last ? "" : "," : last ? "\"" : "\",";
            return "\"" + key + sep + value + end;
        }

        static String makeConditionalProperty( String key, String value, bool last = false, bool raw = false ) {
            return value.length() ? makeProperty( key, value, last, raw ) : "";
        }

        static String makeConditionalProperty( String key, int value, int defval, bool last = false ) {
            return ( value != defval ) ? makeProperty( key, String( value ), last, true ) : "";
        }

        static String makeConditionalProperty( String key, bool value, bool defval, bool last = false ) {
            return ( value != defval ) ? makeProperty( key, String( value ), last, true ) : "";
        }

        static int WifiGetRssiAsQuality( int rssi ) {
            int quality = 0;

            if ( rssi <= -100 ) {
                quality = 0;
            } else if ( rssi >= -50 ) {
                quality = 100;
            } else {
                quality = 2 * ( rssi + 100 );
            }
            return quality;
        }
    };
}  // namespace ustd

#endif // __ESP__