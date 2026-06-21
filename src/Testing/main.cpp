#include "sense_hat_display.h"
#include <chrono>
#include <thread>
#include <iostream>

using namespace std::this_thread;
using namespace std::literals::chrono_literals;

int main(){
    SenseHatDisplay display;

    if(display.available())
    {
        display.StartCountDown(3);

        std::cout << "Hi i work, yay :)"<< std::flush;

        sleep_for(2000ms);

        display.ShowWin();

        sleep_for(2000ms);

        display.ShowLoss();

        sleep_for(2000ms);

        display.ShowErrorMarker();

        sleep_for(2000ms);

        display.ShowRPS(RPS::rock, 1);
        display.ShowRPS(RPS::paper, 1);
        display.ShowRPS(RPS::scissors, 1);
        display.ShowRPS(RPS::reset, 1);
    }


    return;
}