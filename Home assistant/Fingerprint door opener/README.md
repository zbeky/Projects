# Fingerprint-wemos
FPM10A Fingerprint sensor with Wemos D1 mini to unlock doors via relay and connect to MQTT.

Slightly modified version of https://github.com/EverythingSmartHome/fingerprint-mqtt
 - Added sensor blinking for OK/Error
 - Added successful message to topic state
 - Learning timeout when no finger is placed in 10 seconds
 - Turn on relay when finger matches