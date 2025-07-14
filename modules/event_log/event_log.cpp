//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#include "event_log.h"

#include "siren.h"
#include "fire_alarm.h"
#include "user_interface.h"
#include "date_and_time.h"
#include "pc_serial_com.h"
#include "motion_sensor.h"
#include "sd_card.h"
#include "display.h"     

//=====[Declaration of private defines]========================================

//=====[Declaration of private data types]=====================================

typedef struct systemEvent {
    time_t seconds;
    char typeOfEvent[EVENT_LOG_NAME_MAX_LENGTH];
} systemEvent_t;

//=====[Declaration and initialization of public global objects]===============

//=====[Declaration of external public global variables]=======================

//=====[Declaration and initialization of public global variables]=============

//=====[Declaration and initialization of private global variables]============

static bool sirenLastState = OFF;
static bool gasLastState   = OFF;
static bool tempLastState  = OFF;
static bool ICLastState    = OFF;
static bool SBLastState    = OFF;
static bool motionLastState         = OFF;
static int eventsIndex     = 0;
static systemEvent_t arrayOfStoredEvents[EVENT_LOG_MAX_STORAGE];

//=====[Declarations (prototypes) of private functions]========================

static void eventLogElementStateUpdate( bool lastState,
                                        bool currentState,
                                        const char* elementName );

//=====[Implementations of public functions]===================================

void eventLogUpdate()
{
    bool currentState = sirenStateRead();
    eventLogElementStateUpdate( sirenLastState, currentState, "ALARM" );
    sirenLastState = currentState;

    currentState = gasDetectorStateRead();
    eventLogElementStateUpdate( gasLastState, currentState, "GAS_DET" );
    gasLastState = currentState;

    currentState = overTemperatureDetectorStateRead();
    eventLogElementStateUpdate( tempLastState, currentState, "OVER_TEMP" );
    tempLastState = currentState;

    currentState = incorrectCodeStateRead();
    eventLogElementStateUpdate( ICLastState, currentState, "LED_IC" );
    ICLastState = currentState;

    currentState = systemBlockedStateRead();
    eventLogElementStateUpdate( SBLastState ,currentState, "LED_SB" );
    SBLastState = currentState;

    currentState = motionSensorRead();
    eventLogElementStateUpdate( motionLastState ,currentState, "MOTION" );
    motionLastState = currentState;
}

int eventLogNumberOfStoredEvents()
{
    return eventsIndex;
}

void eventLogRead( int index, char* str )
{
    str[0] = '\0';
    strcat( str, "Event = " );
    strcat( str, arrayOfStoredEvents[index].typeOfEvent );
    strcat( str, "\r\nDate and Time = " );
    strcat( str, ctime(&arrayOfStoredEvents[index].seconds) );
    strcat( str, "\r\n" );
}

void eventLogWrite(bool currentState, const char* elementName)
{
    if (strcmp(elementName, "GAS_DET") != 0
     && strcmp(elementName, "OVER_TEMP") != 0) {
        return;
    }

    char eventStr[EVENT_LOG_NAME_MAX_LENGTH] = "";
    strcat(eventStr, elementName);
    strcat(eventStr, currentState ? "_ON" : "_OFF");

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
    char line[80];
    int n = snprintf(line, sizeof(line), "%s  %s\r\n", ts, eventStr);

    arrayOfStoredEvents[eventsIndex].seconds     = now;
    strncpy(arrayOfStoredEvents[eventsIndex].typeOfEvent,
            eventStr, EVENT_LOG_NAME_MAX_LENGTH);
    eventsIndex = (eventsIndex + 1) % EVENT_LOG_MAX_STORAGE;

    pcSerialComStringWrite(line);

    displayCharPositionWrite(0, 3);
    displayStringWrite("                ");  
    displayCharPositionWrite(0, 3);
    displayStringWrite(eventStr); 

    sdCardWriteFile("events.txt", line);
}

bool eventLogSaveToSdCard()
{
    char fileName[SD_CARD_FILENAME_MAX_LENGTH];
    char eventStr[EVENT_STR_LENGTH] = "";
    bool eventsStored = false;

    time_t seconds;
    int i;

    seconds = time(NULL);
    fileName[0] = '\0';

    strftime( fileName, SD_CARD_FILENAME_MAX_LENGTH, 
              "%Y_%m_%d_%H_%M_%S", localtime(&seconds) );
    strcat( fileName, ".txt" );

    for (i = 0; i < eventLogNumberOfStoredEvents(); i++) {
        eventLogRead( i, eventStr );
        if ( sdCardWriteFile( fileName, eventStr ) ){
            pcSerialComStringWrite("Storing event ");
            pcSerialComIntWrite(i+1);
            pcSerialComStringWrite(" in file ");
            pcSerialComStringWrite(fileName);
            pcSerialComStringWrite("\r\n");
            eventsStored = true;
        }
    }

    if ( eventsStored ) {
        pcSerialComStringWrite("File successfully written\r\n\r\n");
    } else {
        pcSerialComStringWrite("There are no events to store ");
        pcSerialComStringWrite("or SD card is not available\r\n\r\n");
    }

    return true;
}

//=====[Implementations of private functions]==================================

static void eventLogElementStateUpdate( bool lastState,
                                        bool currentState,
                                        const char* elementName )
{
    if ( lastState != currentState ) {        
        eventLogWrite( currentState, elementName );       
    }
}
