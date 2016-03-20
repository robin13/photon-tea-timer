#include "application.h"

// Pin definitions
uint16_t SERVO_PIN = A5;
uint16_t BUTTON_PIN = A4;

// variables
bool jiggle_servo_down = true;
bool set_time_mode = false;
bool brewing = false;
bool waiting_for_release = false;

uint32_t time_last_push = 0;
uint32_t time_last_release = 0;
uint32_t brew_finish_time = 0;

uint16_t position_up = 20;
uint16_t position_down = 160;
uint16_t brew_for_minutes = 2;
uint16_t sleep_after = 60000;
int running_since = -1;

Servo tea_servo;

void buttonIRQ()
{
    // Button pushed
    if( digitalRead( BUTTON_PIN ) == LOW ){
        // Manual button caused many bounces below 300ms
        if( waiting_for_release == false and ( millis() - time_last_push ) > 300 ){
            Serial.println( "Button pushed" );
            time_last_push = millis();
            waiting_for_release = true;
            if( set_time_mode == true ){
                brew_for_minutes++;
                Serial.print( "Incrementing brew time to: " );
                Serial.println( brew_for_minutes );
                jiggle_servo_down = true;
            }
        }
    }else{
        // Manual button caused many bounces below 300ms
        if( waiting_for_release == true and ( millis() - time_last_release ) > 300 ){
            waiting_for_release = false;
            time_last_release = millis();
            Serial.println( "Button released" );
            if( set_time_mode == false )
            {
                Serial.println( "Start brewing" );
                brew_finish_time = millis() + ( brew_for_minutes * 60000 );
                brewing = true;
                jiggle_servo_down = true;
            }
        }
    }
    running_since = millis();
}


// Move the servo to the down position
// Do a little "jitter" so that even if moving from down->down it is visible
// that "something was done"
void moveServoDown( uint16_t offset )
{
    tea_servo.write( position_down );
    delay( offset * 10 );
    tea_servo.write( position_down - offset );
    delay( offset * 10 );
    tea_servo.write( position_down );
}


void setup() {
    Serial.begin(9600);

    Serial.println( "Running setup" );

    // Set the pins
    tea_servo.attach( SERVO_PIN );
    pinMode( BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt( BUTTON_PIN, buttonIRQ, CHANGE );

    running_since = millis();
    WiFi.off();
}

void loop() {
    // after 2 seconds button push, enter set_time_mode
    if( set_time_mode == false and waiting_for_release == true and ( millis() - time_last_push ) > 2000 )
    {
        Serial.println( "set_time_mode => true" );
        set_time_mode = true;
        brew_for_minutes = 0;
        moveServoDown( 90 );
        time_last_push = millis();
    }

    if( set_time_mode == true and waiting_for_release == false and ( millis() - time_last_push ) > 5000 )
    {
        Serial.println( "set_time_mode => false" );
        set_time_mode = false;

        // brew_for_minutes of 0 (no time set) is illegal, so set to 1
        if( brew_for_minutes == 0 ){
            brew_for_minutes = 1;
        }
        for( uint16_t i = 0; i < brew_for_minutes; i++ ){
            moveServoDown( 30 );
        }
    }

    if( jiggle_servo_down == true ){
        moveServoDown( 10 );
        jiggle_servo_down = false;
    }

    // If coming back from sleep
    if( running_since < 0 ){
        WiFi.off();
        pinMode( BUTTON_PIN, INPUT_PULLUP);
        attachInterrupt( BUTTON_PIN, buttonIRQ, CHANGE );
        running_since = millis();
        brewing = false;
        jiggle_servo_down = true;
    }

    // brew_for_minutes is in minutes, so it should eventually be * 60000
    if( brewing == true and millis() > brew_finish_time )
    {
        Serial.println( "Tea is finished" );
        tea_servo.write( position_up );
        Particle.connect();
        while( not Particle.connected() ){
            delay( 100 );
        }
        String json = "{\"tea\":\"finished\",\"brew_time\":";
        json.concat( brew_for_minutes );
        json.concat( "}" );
        Particle.publish( "tea", json );
        Serial.println( "Going to sleep now..." );
        running_since = -1;
        brewing = false;
        // Give the servo time to act before sleeping
        delay( 1000 );
        System.sleep( BUTTON_PIN, FALLING );
    }
    if( brewing == false and ( millis() - running_since ) > sleep_after ){
        Serial.println( "Going to sleep now..." );
        running_since = -1;
        delay( 10 );
        System.sleep( BUTTON_PIN, FALLING );
    }
}

