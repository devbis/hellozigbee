#include "LEDHandler.h"

const LEDProgramEntry BREATHE_EFFECT[] = 
{
    {LED_CMD_MOVE_TO_LEVEL, 200, 10},   // Gradually move to the bright level
    {LED_CMD_PAUSE, 10, 0},             // Stay there for a half a second
    {LED_CMD_MOVE_TO_LEVEL, 50, 10},    // Gradually move to the dimmed level
    {LED_CMD_PAUSE, 10, 0},             // Stay there for a half a second

    {LED_CMD_REPEAT, 4, 10},            // Jump 4 steps back, Repeat 10 times

    {LED_CMD_MOVE_TO_LEVEL, 0, 10},     // Switch off

    {LED_CMD_STOP, 0, 0},
};


LEDHandler::LEDHandler()
{

}

void LEDHandler::init(uint8 timer)
{
    pin.init(timer);
    curLevel = 0;
    targetLevel = 0;
    increment = 0;

    handlerState = STATE_IDLE;

    programPtr = BREATHE_EFFECT;
    programIterations = 0;
}

void LEDHandler::handleStateIncrementing()
{
    int level = (int)curLevel + (int)increment;

    if(level > targetLevel)
        level = targetLevel;

    if(level > 255)
        level = 255;

    if(level == targetLevel)
        handlerState = STATE_IDLE;

    curLevel = (uint8)level;
    pin.setLevel(curLevel);
}

void LEDHandler::handleStateDecrementing()
{
    int level = (int)curLevel - (int)increment;

    if(level < 0)
        level = 0;

    if(level < targetLevel)
        level = targetLevel;

    if(level == targetLevel)
        handlerState = STATE_IDLE;

    curLevel = (uint8)level;
    pin.setLevel(curLevel);
}

void LEDHandler::handleStatePause()
{
    pauseCycles--;
    if(pauseCycles == 0)
        handlerState = STATE_IDLE;
}

void LEDHandler::handleStateMachine()
{
    switch(handlerState)
    {
        case STATE_INCREMENTING:
            handleStateIncrementing();
            break;

        case STATE_DECREMENTING:
            handleStateDecrementing();
            break;

        case STATE_PAUSE:
            handleStatePause();
            break;

        case STATE_IDLE:
        default:
            break;
    }
}

void LEDHandler::handleProgramCommand()
{
    LEDProgramEntry command = *programPtr;
    programPtr++;

    switch(command.command)
    {
        // Abandon program
        case LED_CMD_STOP:  
            programPtr = NULL;
            break;

        // Schedule gradual movement to desired level (up or down)
        case LED_CMD_MOVE_TO_LEVEL:
            targetLevel = command.param1;
            increment = command.param2;
            handlerState = curLevel < targetLevel ? STATE_INCREMENTING : STATE_DECREMENTING;
            break;

        // Schedule a short pause
        case LED_CMD_PAUSE:
            pauseCycles = command.param1;
            handlerState = STATE_PAUSE;
            break;

        // Repeat few previous commands (number is in param1), until iterations counter matches param2
        case LED_CMD_REPEAT:
            programIterations++;

            if(programIterations >= command.param2)
                break;

            programPtr -= (command.param1 + 1);
            break;

        default:
            break;
    }
}

void LEDHandler::update()
{
    handleStateMachine();

    if(handlerState == STATE_IDLE && programPtr != NULL)
        handleProgramCommand();
}