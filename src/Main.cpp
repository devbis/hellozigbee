extern "C"
{
    #include "AppHardwareApi.h"
    #include "dbg.h"
    #include "dbg_uart.h"
    #include "portmacro.h"
    #include "pwrm.h"
    #include "PDM.h"

    // Local configuration and generated files
    #include "pdum_gen.h"
    #include "zps_gen.h"
}

#include "Queue.h"
#include "ButtonsTask.h"
#include "SwitchEndpoint.h"
#include "EndpointManager.h"
#include "BasicClusterEndpoint.h"
#include "ZigbeeDevice.h"
#include "ZCLTimer.h"
#include "LEDTask.h"
#include "BlinkTask.h"
#include "DumpFunctions.h"

const uint8 HEARTBEAT_LED_PIN = 4;

const uint8 SWITCH1_LED_PIN = 17;
const uint8 SWITCH2_LED_PIN = 12;

const uint8 SWITCH1_BTN_BIT = 1;
const uint32 SWITCH1_BTN_MASK = 1UL << SWITCH1_BTN_BIT;
const uint8 SWITCH2_BTN_BIT = 2;
const uint32 SWITCH2_BTN_MASK = 1UL << SWITCH2_BTN_BIT;

// Pre-configured Link Key
// TODO: Move it elsewhere when Touch Link functionality is implemented
uint8 s_au8LnkKeyArray[16] __attribute__ ((section (".ro_se_lnkKey")))
= { 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x5a, 0x69, 0x67, 0x62, 0x65, 0x65, 0x30,
		0x30, 0x30, 0x30, 0x30 };


// Hidden funcctions (exported from the library, but not mentioned in header files)
extern "C"
{
    PUBLIC uint8 u8PDM_CalculateFileSystemCapacity(void);
    PUBLIC uint8 u8PDM_GetFileSystemOccupancy(void);
    extern void zps_taskZPS(void);
}


// 6 timers are:
// - 1 in ButtonTask
// - 1 in LEDTask
// - 1 in Heartbeat BlinkTask
// - 1 in PollTask
// - 1 is ZCL timer
// Note: if not enough space in this timers array, some of the functions (e.g. network joining) may not work properly
ZTIMER_tsTimer timers[6 + BDB_ZTIMER_STORAGE];


struct Context
{
    BasicClusterEndpoint basicEndpoint;
    SwitchEndpoint switch1;
    SwitchEndpoint switch2;
};


extern "C" void __cxa_pure_virtual(void) __attribute__((__noreturn__));
extern "C" void __cxa_deleted_virtual(void) __attribute__((__noreturn__));

void __cxa_pure_virtual(void)
{
  DBG_vPrintf(TRUE, "!!!!!!! Pure virtual function call.\n");
  while (1)
    ;
}


extern "C" PUBLIC void vISR_SystemController(void)
{
    // clear pending DIO changed bits by reading register
    uint8 wakeStatus = u8AHI_WakeTimerFiredStatus();
    uint32 dioStatus = u32AHI_DioInterruptStatus();

    DBG_vPrintf(TRUE, "In vISR_SystemController\n");

    if(ButtonsTask::getInstance()->handleDioInterrupt(dioStatus))
    {
        DBG_vPrintf(TRUE, "=-=-=- Button interrupt dioStatus=%04x\n", dioStatus);
        PWRM_vWakeInterruptCallback();
    }

    if(wakeStatus & E_AHI_WAKE_TIMER_MASK_1)
    {
        DBG_vPrintf(TRUE, "=-=-=- Wake Timer Interrupt\n");
        PWRM_vWakeInterruptCallback();
    }
}

void vfExtendedStatusCallBack (ZPS_teExtendedStatus eExtendedStatus)
{
    DBG_vPrintf(TRUE,"ERROR: Extended status %x\n", eExtendedStatus);
}

PUBLIC void wakeCallBack(void)
{
    DBG_vPrintf(TRUE, "=-=-=- wakeCallBack()\n");
}

PRIVATE void APP_vTaskSwitch(Context * context)
{
    if(ButtonsTask::getInstance()->canSleep() &&
       ZigbeeDevice::getInstance()->canSleep())
    {
        static pwrm_tsWakeTimerEvent wakeStruct;
        PWRM_teStatus status = PWRM_eScheduleActivity(&wakeStruct, 15 * 32000, wakeCallBack);
        if(status != PWRM_E_TIMER_RUNNING)
            DBG_vPrintf(TRUE, "=-=-=- Scheduling enter sleep mode... status=%d\n", status);
    }
}

class DebugInput
{
    static const int BUF_SIZE = 32;
    char buf[BUF_SIZE];
    char * ptr;
    bool hasData;

public:
    DebugInput()
    {
        reset();
    }

    void handleDebugInput()
    {
        // Avoid buffer overrun
        if(ptr >= buf + BUF_SIZE)
            return;

        // Receive the next symbol, if any
        while(u16AHI_UartReadRxFifoLevel(E_AHI_UART_0) > 0)
        {
            char ch;
            u16AHI_UartBlockReadData(E_AHI_UART_0, (uint8*)&ch, 1);

            if(ch == '\r' || ch == '\n')
            {
                *ptr = 0;
                hasData = true;
            }
            else
                *ptr = ch;

            ptr++;
        }
    }

    bool hasCompletedLine() const
    {
        return hasData;
    }

    bool matchCommand(const char * command) const
    {
        int len = strlen(command);
        if(strncmp(command, buf, len) == 0 && buf[len] == 0)
            return true;

        return false;
    }

    void reset()
    {
        ptr = buf;
        hasData = false;
    }
};

PRIVATE void APP_vHandleDebugInput(DebugInput & debugInput)
{
    debugInput.handleDebugInput();
    if(debugInput.hasCompletedLine())
    {
        if(debugInput.matchCommand("BTN1_PRESS"))
        {
            ButtonsTask::getInstance()->setButtonsOverride(SWITCH1_BTN_MASK);
            DBG_vPrintf(TRUE, "Matched BTN1_PRESS\n");
        }

        if(debugInput.matchCommand("BTN2_PRESS"))
        {
            ButtonsTask::getInstance()->setButtonsOverride(SWITCH2_BTN_MASK);
            DBG_vPrintf(TRUE, "Matched BTN2_PRESS\n");
        }

        if(debugInput.matchCommand("BTN1_RELEASE") || debugInput.matchCommand("BTN2_RELEASE"))
        {
            ButtonsTask::getInstance()->setButtonsOverride(0);
            DBG_vPrintf(TRUE, "Matched BTNx_RELEASE\n");
        }

        debugInput.reset();
    }
}

extern "C" PUBLIC void vAppMain(void)
{
    // Initialize the hardware
    TARGET_INITIALISE();
    SET_IPL(0);
    portENABLE_INTERRUPTS();

    // Initialize UART
    vAHI_UartSetRTSCTS(E_AHI_UART_0, FALSE);
    DBG_vUartInit(DBG_E_UART_0, DBG_E_UART_BAUD_RATE_115200);
    DebugInput debugInput;

    // Print welcome message
    DBG_vPrintf(TRUE, "\n---------------------------------------------------\n");
    DBG_vPrintf(TRUE, "Initializing Hello Zigbee Platform\n");
    DBG_vPrintf(TRUE, "---------------------------------------------------\n\n");

    // Initialize PDM
    DBG_vPrintf(TRUE, "vAppMain(): init PDM...  ");
    PDM_eInitialise(0);
    DBG_vPrintf(TRUE, "PDM Capacity %d Occupancy %d\n",
            u8PDM_CalculateFileSystemCapacity(),
            u8PDM_GetFileSystemOccupancy() );

    // Initialize power manager and sleep mode
    DBG_vPrintf(TRUE, "vAppMain(): init PWRM...\n");
    PWRM_vInit(E_AHI_SLEEP_OSCON_RAMON);

    // PDU Manager initialization
    DBG_vPrintf(TRUE, "vAppMain(): init PDUM...\n");
    PDUM_vInit();

    // Init timers
    DBG_vPrintf(TRUE, "vAppMain(): init software timers...\n");
    ZTIMER_eInit(timers, sizeof(timers) / sizeof(ZTIMER_tsTimer));

    // Init tasks
    DBG_vPrintf(TRUE, "vAppMain(): init tasks...\n");
    ButtonsTask::getInstance();

    // Initialize periodic tasks
    DBG_vPrintf(TRUE, "vAppMain(): init periodic tasks...\n");
    ZCLTimer zclTimer;
    zclTimer.init();
    zclTimer.startTimer(1000);
    LEDTask::getInstance()->start();
    BlinkTask blinkTask;
    blinkTask.init(HEARTBEAT_LED_PIN);

    // Set up a status callback
    DBG_vPrintf(TRUE, "vAppMain(): init extended status callback...\n");
    ZPS_vExtendedStatusSetCallback(vfExtendedStatusCallBack);

    DBG_vPrintf(TRUE, "vAppMain(): Registering endpoint objects\n");
    Context context;
    context.switch1.setPins(SWITCH1_BTN_MASK);
    context.switch2.setPins(SWITCH2_BTN_MASK);
    EndpointManager::getInstance()->registerEndpoint(HELLOENDDEVICE_BASIC_ENDPOINT, &context.basicEndpoint);
    EndpointManager::getInstance()->registerEndpoint(HELLOENDDEVICE_SWITCH1_ENDPOINT, &context.switch1);
    EndpointManager::getInstance()->registerEndpoint(HELLOENDDEVICE_SWITCH2_ENDPOINT, &context.switch2);

    // Init the ZigbeeDevice, AF, BDB, and other network stuff
    ZigbeeDevice::getInstance();

    // Groups debug (TODO: Cleanup this)
    vDisplayGroupsTable();

    // Print Initialization finished message
    DBG_vPrintf(TRUE, "\n---------------------------------------------------\n");
    DBG_vPrintf(TRUE, "Initialization of the Hello Zigbee Platform Finished\n");
    DBG_vPrintf(TRUE, "---------------------------------------------------\n\n");

    // Start ZigbeeDevice and rejoin the network (if was joined)
    ZigbeeDevice::getInstance()->rejoinNetwork();

    DBG_vPrintf(TRUE, "\nvAppMain(): Starting the main loop\n");
    while(1)
    {
        zps_taskZPS();

        bdb_taskBDB();

        ZTIMER_vTask();

        APP_vHandleDebugInput(debugInput);

        APP_vTaskSwitch(&context);

        vAHI_WatchdogRestart();

        PWRM_vManagePower();
    }
}

static PWRM_DECLARE_CALLBACK_DESCRIPTOR(PreSleep);
static PWRM_DECLARE_CALLBACK_DESCRIPTOR(Wakeup);

PWRM_CALLBACK(PreSleep)
{
    DBG_vPrintf(TRUE, "Going to sleep..\n\n");

    // Save the MAC settings (will get lost though if we don't preserve RAM)
    vAppApiSaveMacSettings();

    // Put ZTimer module to sleep (stop tick timer)
    ZTIMER_vSleep();

    // Disable UART (if enabled)
    DBG_vUartFlush();
    vAHI_UartDisable(E_AHI_UART_0);

    // clear interrupts
    u32AHI_DioWakeStatus();
}

PWRM_CALLBACK(Wakeup)
{
    // Stabilise the oscillator
    while (bAHI_GetClkSource() == TRUE);

    // Now we are running on the XTAL, optimise the flash memory wait states
    vAHI_OptimiseWaitStates();

    // Re-initialize Debug UART
    DBG_vUartInit(DBG_E_UART_0, DBG_E_UART_BAUD_RATE_115200);
    DBG_vPrintf(TRUE, "\nWaking...\n");
    DBG_vUartFlush();

    // Restore Mac settings (turns radio on)
    vMAC_RestoreSettings();

    // Re-initialize hardware and interrupts
    TARGET_INITIALISE();
    SET_IPL(0);
    portENABLE_INTERRUPTS();

    // Wake the timers
    ZTIMER_vWake();

    // Poll the parent router for zigbee messages
    ZigbeeDevice::getInstance()->handleWakeUp();
}

extern "C" void vAppRegisterPWRMCallbacks(void)
{
    PWRM_vRegisterPreSleepCallback(PreSleep);
    PWRM_vRegisterWakeupCallback(Wakeup);	
}
