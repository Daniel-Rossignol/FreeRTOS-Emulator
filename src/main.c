#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_Font.h"

#include "AsyncIO.h"

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)

#define DRAW_OFFSET_X SCREEN_WIDTH/2
#define DRAW_OFFSET_Y SCREEN_HEIGHT/2
#define ROT_RADIUS 50
#define CIRCLE_RADIUS 10
#define CUBE_WIDTH 20

#define DEBOUNCE_DELAY 1

static TaskHandle_t running_the_display_task = NULL;
static TaskHandle_t using_buttons_task = NULL;
static TaskHandle_t using_the_mouse_task = NULL;


typedef struct buttons_buffer {
    unsigned char buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };

void xGetButtonInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
        xSemaphoreGive(buttons.lock);
    }
}

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

void running_the_display(void *pvParameters)
{
    //thread that is allowed to draw needs this
    tumDrawBindThread();

    //arbitrary points of the triangle
    static coord_t tr_points[3] = {{DRAW_OFFSET_X - 10, DRAW_OFFSET_Y + 10},
                             {DRAW_OFFSET_X - 10, DRAW_OFFSET_Y - 10}, 
                             {DRAW_OFFSET_X + 10, DRAW_OFFSET_Y + 10}};

    //angle of rotation
    static float phi = 0;
    //for sliding text at the top
    static int text_offset = 0;
    //amount by which the text will be moved (<0 for left)
    static int text_dir = 10;
    while (1) 
    {
        tumDrawClear(White); // Clear screen
        
    
        phi += 0.05; //increment the rotational angle
        //draw Circle and Box with offset from rotation
        tumDrawCircle(DRAW_OFFSET_X + cos(phi)*ROT_RADIUS,
                        DRAW_OFFSET_Y + sin(phi)*ROT_RADIUS,
                         CIRCLE_RADIUS, Black);
        tumDrawFilledBox(DRAW_OFFSET_X - CUBE_WIDTH/2 + cos(phi + 3.14)*ROT_RADIUS,
                            DRAW_OFFSET_Y - CUBE_WIDTH/2 + sin(phi + 3.14)*ROT_RADIUS,
                            CUBE_WIDTH, CUBE_WIDTH, Red);
        //stationary triangle in middle
        tumDrawTriangle(tr_points, Blue);

        //bottom text
        tumDrawText("Covid", DRAW_OFFSET_X - 20, DRAW_OFFSET_Y + 100, Pink);

        //top sliding text
        //check if offscreen while hotfixing the width of the text
        if(text_offset + 50 >SCREEN_WIDTH || text_offset<0)
        {
            text_dir*=-1;
        }
        text_offset += text_dir;

        tumDrawText("Fuck", text_offset, DRAW_OFFSET_Y - 100, Pink);


        tumDrawUpdateScreen(); // Refresh the screen to draw string

        // Basic sleep of 20 milliseconds
        vTaskDelay((TickType_t)20);
    }
}


void using_buttons(void *pvParameters)
{
    tumDrawBindThread();

    static unsigned char keys[4] = {SDL_SCANCODE_A,SDL_SCANCODE_B,SDL_SCANCODE_C,SDL_SCANCODE_D};

    //array to store how many times each button was pressed
    static int button_counter[4] = {0,0,0,0};

    static char output_text[20];

    //debounce vars
    static TickType_t last_debounce_time[4] = {0,0,0,0};
    static int last_button_state[4] = {0,0,0,0};
    static int button_state[4] = {0,0,0,0};

    static int my_temp_button = 0;

    while(1)
    {
        tumEventFetchEvents(FETCH_EVENT_NONBLOCK); // Query events backend for new events, ie. button presses
        xGetButtonInput(); // Update global input

        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(Q)]) { // Equiv to SDL_SCANCODE_Q
                exit(EXIT_SUCCESS);
            }

            //increment counter by one if key is pressed
            //combating debounce read more at https://www.arduino.cc/en/Tutorial/BuiltInExamples/Debounce
            for(int i = 0; i < 4; i++){

                my_temp_button = buttons.buttons[keys[i]];

                if(my_temp_button != last_button_state[i])
                {
                    last_debounce_time[i] = xTaskGetTickCount();
                }
                if((xTaskGetTickCount() - last_debounce_time[i]) > DEBOUNCE_DELAY){
                    if(my_temp_button != button_state[i])
                    {
                        button_state[i] = my_temp_button;
                        button_counter[i] += my_temp_button;
                    }
                }
                last_button_state[i] = my_temp_button;
            }
            //end of semaphore
            xSemaphoreGive(buttons.lock);
        }

        //reset the values if left mouse button is pressed
        if(tumEventGetMouseLeft())
        {
            for(int i = 0; i < 4; i++){
                button_counter[i] = 0;
            }
        }

        tumDrawClear(White); // Clear screen

        //splicing strings for printing to screen
        //61 is the offset between ASCII 'A' and SDL_SCANCODE_A
        for(int i = 0; i < 4; i++){
            sprintf(output_text, "%c: %d",keys[i] + 61, button_counter[i]);
            tumDrawText(output_text,100,100 + i*50, Black);
        }


        //quit text
        tumDrawText("press q to quit and left click resets the values",10,10, Red);


        tumDrawUpdateScreen(); // Refresh the screen to draw string

        // Basic sleep of 1 milliseconds
        vTaskDelay((TickType_t)1);
    }
        


}

void using_the_mouse(void *pvParameters)
{   
    //only one thread can draw
    tumDrawBindThread();

    static char output_text[20];

    while (1) 
    {
        tumEventFetchEvents(FETCH_EVENT_NONBLOCK); // Query events backend for new events, ie. button presses
        xGetButtonInput(); // Update global input

        //q to quit
        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
            if (buttons.buttons[KEYCODE(Q)]) { // Equiv to SDL_SCANCODE_Q
                exit(EXIT_SUCCESS);
            }

        }

        tumDrawClear(White); // Clear screen

        //printing Mouse location next to mouse cursor
        sprintf(output_text, "Mouse x: %d", tumEventGetMouseX());
        tumDrawText(output_text,tumEventGetMouseX(),tumEventGetMouseY(), Black);

        sprintf(output_text, "Mouse y: %d", tumEventGetMouseY());
        tumDrawText(output_text,tumEventGetMouseX(),tumEventGetMouseY()+30, Black);



        tumDrawUpdateScreen(); // Refresh the screen to draw string

        // Basic sleep of 20 milliseconds
        vTaskDelay((TickType_t)20);

    }
    


}



int main(int argc, char *argv[])
{
    char *bin_folder_path = tumUtilGetBinFolderPath(argv[0]);


    //deciding which subexercise to run
    printf("Enter which exercise to run (1/2/3):");
    char in = fgetc(stdin);

    //Draw and Event init
    if (tumDrawInit(bin_folder_path)) {
        PRINT_ERROR("Failed to initialize drawing");
        goto err_init_drawing;
    }

    if (tumEventInit()) {
        PRINT_ERROR("Failed to initialize events");
        goto err_init_events;
    }


    buttons.lock = xSemaphoreCreateMutex(); // Locking mechanism
    if (!buttons.lock) {
        PRINT_ERROR("Failed to create buttons lock");
        goto err_buttons_lock;
    }


    switch(in) {
        case '1': 
            printf("Starting exercise 2.1");
            
            if (xTaskCreate(running_the_display, "Running_display", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &running_the_display_task) != pdPASS) {
                goto err_task;
            }
            break;
        case '2': printf("Starting exercise 2.2");
            printf("Starting exercise 2.1");
            
            if (xTaskCreate(using_buttons, "using buttons", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &using_buttons_task) != pdPASS) {
                goto err_task;
            }
            break;
        case '3': printf("Starting exercise 2.3");
            if (xTaskCreate(using_the_mouse, "using buttons", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &using_the_mouse_task) != pdPASS) {
                goto err_task;
            }
             break;
        default:
            return -1;
    }

    

    // if (xTaskCreate(vDemoTask, "DemoTask", mainGENERIC_STACK_SIZE * 2, NULL,
    //                 mainGENERIC_PRIORITY, &DemoTask) != pdPASS) {
    //     goto err_demotask;
    // }

    vTaskStartScheduler();

    return EXIT_SUCCESS;

err_task:
err_buttons_lock:
    vSemaphoreDelete(buttons.lock);
err_init_events:
    tumEventExit();
err_init_drawing:
    tumDrawExit();
    return EXIT_FAILURE;
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vMainQueueSendPassed(void)
{
    /* This is just an example implementation of the "queue send" trace hook. */
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vApplicationIdleHook(void)
{
#ifdef __GCC_POSIX__
    struct timespec xTimeToSleep, xTimeSlept;
    /* Makes the process more agreeable when using the Posix simulator. */
    xTimeToSleep.tv_sec = 1;
    xTimeToSleep.tv_nsec = 0;
    nanosleep(&xTimeToSleep, &xTimeSlept);
#endif
}
