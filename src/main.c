#include <stdio.h>
#include <string.h>
#include <math.h>
#include "stm32f0xx.h"
#include "ff.h"
#include "lcd.h"
#include "tty.h"
#include "commands.h"
#include "invader.h"
#include "midi.h"
#include "fifo.h"

#define FIFOSIZE 16
#define NUM 100
#define RATE 40000
#include <step-array.h>
struct {
    uint8_t note;
    uint8_t chan;
    uint8_t volume;
    int     step;
    int     offset;
} voice[15];

extern uint8_t midifile[];
MIDI_Player *mp;
short int wavetable[NUM];
int time = 10;
int shot_fired = 0;
int game_started = 0;
int player_pos = 114;
int laser_y = 280;
int game_over = 0;
int last_player_pos;
int enemy[5] = {1, 1, 1, 1, 1};
int enemy2[5] = {1, 1, 1, 1, 1};
int enemy3[5] = {1, 1, 1, 1, 1};
int enemy4[5] = {1, 1, 1, 1, 1};
int enemy5[5] = {1, 1, 1, 1, 1};
int enemy6[5] = {1, 1, 1, 1, 1};
int enemy7[5] = {1, 1, 1, 1, 1};
int enemy8[5] = {1, 1, 1, 1, 1};
int enemy9[5] = {1, 1, 1, 1, 1};
int enemy10[5] = {1, 1, 1, 1, 1};
int enemy11[5] = {1, 1, 1, 1, 1};
int score;
char serfifo[FIFOSIZE];
int seroffset = 0;
int time;

//int enemies[5][11] = {
//        {1,1,1,1,1,1,1,1,1,1},
//        {1,1,1,1,1,1,1,1,1,1},
//        {1,1,1,1,1,1,1,1,1,1},
//        {1,1,1,1,1,1,1,1,1,1},
//        {1,1,1,1,1,1,1,1,1,1}
//};
//
//int x_positions[11] = {9, 29, 49, 69, 89, 109, 129, 149, 169, 189, 209};
//int y_positions[5] = {160, 140, 120, 100, 80};

void init_wavetable(void) {
    for(int i=0; i < NUM; i++) {
        wavetable[i] = 32767 * sin(2 * M_PI * i / NUM);
    }
}

void note_on(int time, int chan, int key, int velo)
{
    if (velo == 0) {
        note_off(time, chan, key, velo);
        return;
    }

    for(int i=0; i < sizeof voice / sizeof voice[0]; i++)
        if (voice[i].step == 0) {
          // configure this voice to have the right step and volume
          voice[i].step = step[key];
          voice[i].note = key;
          voice[i].volume = velo;
          break;
    }

}

void note_off(int time, int chan, int key, int velo)
{
  for(int i=0; i < sizeof voice / sizeof voice[0]; i++)
    if (voice[i].step != 0 && voice[i].note == key) {
      // turn off this voice
      voice[i].step = 0;
      voice[i].note = 0;
      voice[i].volume = 0;
      //voice[i].offset = 0;
      break;
    }
}

void init_TIM2(int rate) {
    RCC->APB1ENR |= 0x1;
    TIM2->PSC = 48-1;
    TIM2->ARR = rate - 1;
    TIM2->CR1 |= 0x1;
    TIM2->DIER |= TIM_DIER_UIE;
    NVIC->ISER[0] = 1 << TIM2_IRQn;
    //set_tempo(rate);
}

void TIM2_IRQHandler(void)
{
    // TODO: Remember to acknowledge the interrupt right here!
    TIM2->SR &= ~TIM_SR_UIF;
    if (mp->nexttick >= MAXTICKS)
                midi_init(midifile);
    midi_play();
}

void init_TIM6() {
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    TIM6->PSC = 48 - 1;
    TIM6->ARR = 1000000 / RATE - 1;
    TIM6->DIER |= TIM_DIER_UIE;
    TIM6->CR1 |= TIM_CR1_CEN;
    NVIC->ISER[0] |= 1 << TIM6_DAC_IRQn;

}

void TIM6_DAC_IRQHandler(void) {
    TIM6->SR &= ~TIM_SR_UIF;
    DAC->SWTRIGR |= DAC_SWTRIGR_SWTRIG1;

    int sample = 0;
    for(int i=0; i < sizeof voice / sizeof voice[0]; i++) {
        if (voice[i].step != 0) {
            sample += (wavetable[voice[i].offset>>16] * voice[i].volume);
            voice[i].offset += voice[i].step;
            if ((voice[i].offset >> 16) >= sizeof wavetable / sizeof wavetable[0])
                voice[i].offset -= (sizeof wavetable / sizeof wavetable[0]) << 16;
        }
    }
    sample = (sample >> 16) + 2048;
    DAC->DHR12R1 = sample;

}

void init_TIM3() {
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    TIM3->PSC = 48000 - 1;
    TIM3->ARR = 1000-1;
    TIM3->DIER |= TIM_DIER_UIE;
    TIM3->CR1 |= TIM_CR1_CEN;
    NVIC->ISER[0] |= 1 << TIM3_IRQn;

}

void TIM3_IRQHandler()
{
    TIM3->SR &= ~TIM_SR_UIF;

    char score_h[3];
    char time_h[3];
    itoa(time, time_h, 10);
    itoa(score, score_h, 10);
    if (time >= 0)
    {
        LCD_DrawFillRectangle(215, 10, 240, 26, BLACK);
    LCD_DrawString(215, 10, WHITE, BLACK, time_h, 16, 1);
    LCD_DrawFillRectangle(55, 10, 80, 26, BLACK);
    LCD_DrawString(55, 10, WHITE, BLACK, score_h, 16, 1);

    //nano_wait(1000000000);

    time -= 1;
    }

    if (time == -1 && game_over == 0)
    {
        LCD_Clear(0);
        LCD_DrawString(90, 90, WHITE, BLACK, "GAME OVER", 16, 1);
        game_over = 1;
        TIM7->CR1 &= ~TIM_CR1_CEN;
        TIM15->CR1 &= ~TIM_CR1_CEN;
        RCC->APB1ENR &= ~RCC_APB1ENR_TIM3EN;
    }

    else if(game_over == 0 && score == 550)
    {
        time = -2;
        game_over = 1;
        LCD_DrawFillRectangle(0, 30, 240, 290, BLACK);
        LCD_DrawString(90, 90, WHITE, BLACK, "YOU WON!!!", 16, 1);
        TIM15->CR1 &= ~TIM_CR1_CEN;
        RCC->APB1ENR &= ~RCC_APB1ENR_TIM3EN;
    }
}

void init_TIM7() {
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
    TIM7->PSC = 48000 - 1;
    TIM7->ARR = 40 - 1;
    TIM7->DIER |= TIM_DIER_UIE;
    TIM7->CR1 &= ~TIM_CR1_CEN;
    NVIC->ISER[0] |= 1 << TIM7_IRQn;
}

void TIM7_IRQHandler()
{
    TIM7->SR &= ~TIM_SR_UIF;
    if ((GPIOC->IDR & GPIO_IDR_0) == 1) {
        if(player_pos < 240 -14) {
            LCD_DrawFillRectangle(player_pos, 290, player_pos + 12, 290 + 8, BLACK);
            player_pos += 4;
            LCD_DrawPicture(player_pos, 290, (const Picture *)&player);
        }
    }
    if ((GPIOC->IDR & GPIO_IDR_4) == 0x10) {
        if (player_pos > 2) {
            LCD_DrawFillRectangle(player_pos, 290, player_pos + 12, 290 + 8, BLACK);
            player_pos -= 4;
            LCD_DrawPicture(player_pos, 290, (const Picture *)&player);
        }
    }
}

void init_TIM15() {
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;
    TIM15->PSC = 48000 - 1;
    TIM15->ARR = 25 - 1;
    TIM15->DIER |= TIM_DIER_UIE;
    TIM15->CR1 |= TIM_CR1_CEN;
    NVIC->ISER[0] |= 1 << TIM15_IRQn;
}

void TIM15_IRQHandler()
{
    TIM15->SR &= ~TIM_SR_UIF;

    if ((GPIOC->IDR & GPIO_IDR_2) == 0x4 && shot_fired == 0) {
        if (game_started == 0) {
            start_game();
            game_started = 1;
            TIM7->CR1 |= TIM_CR1_CEN;
            shot_fired = 0;
        }
        else {
            shot_fired = 1;
            last_player_pos = player_pos;
        }
    }

    if (shot_fired == 1) {

        LCD_DrawLine(last_player_pos+5, laser_y, last_player_pos + 5, laser_y-15, WHITE);
        LCD_DrawFillRectangle(last_player_pos+5, laser_y-5, last_player_pos + 5, laser_y +5, BLACK);
        laser_y -= 5;
        shot_hit();

    }

    if (laser_y == 40) { //if missed
       shot_fired = 0;
       laser_y = 280;
       LCD_DrawFillRectangle(1, 25, 239, 55, BLACK);
    }

//    if (game_started == 1 && (GPIOC->IDR & GPIO_IDR_2) == 0x4)
//    {
//        int laser_y = 280;
//        for (laser_y = 280; laser_y > 40; laser_y--)
//        {
//
//            //LCD_DrawLine(player_pos+5, laser_y-1, player_pos+5, laser_y-16, WHITE);
//        }
//        LCD_DrawFillRectangle(1, 25, 239, 55, BLACK);
//        LCD_DrawLine(player_pos + 5, laser_y, player_pos + 5, laser_y - 15, WHITE);
//        LCD_DrawFillRectangle(player_pos + 5, laser_y - 5, player_pos + 5, laser_y + 2, BLACK);
//        LCD_DrawLine(player_pos + 5, laser_y-2, player_pos + 5, laser_y - 30, WHITE);
//        }
//        LCD_DrawFillRectangle(player_pos + 5, laser_y-2, player_pos + 5, laser_y + 2, BLACK);
}

void start_game()
{
    LCD_Clear(BLACK);

    score = 0;
    char score_h[3];
    itoa(score, score_h, 10);
    LCD_DrawString(5, 10, WHITE, BLACK, "SCORE:", 16, 1);
    LCD_DrawString(55, 10, WHITE, BLACK, score_h, 16, 1);


    LCD_DrawString(170, 10, WHITE, BLACK, "TIME:", 16, 1);
    init_TIM3();


    LCD_DrawPicture(player_pos, 290, (const Picture *)&player);
    for (int y = 100 ; y < 116; y += 15)
    {
        for (int x = 15; x < 235; x += 20)
        {
            LCD_DrawPicture(x, y, (const Picture*)&alien3);
        }
    }
    for (int y = 130 ; y < 146; y += 15)
    {
        for (int x = 15; x < 235; x += 20)
        {
            LCD_DrawPicture(x, y, (const Picture*)&alien2);
        }
    }
    for (int y = 160 ; y < 171; y += 15)
    {
        for (int x = 15; x < 235; x += 20)
        {
            LCD_DrawPicture(x, y, (const Picture*)&alien1);
        }
    }
}

void shot_hit()
{
    //enemy1
    if(last_player_pos < 21 && last_player_pos > 9 && laser_y < 175)
    {
        if(enemy[4] == 1)
        {
            LCD_DrawFillRectangle(14, 160, 14+14, 160+10, BLACK);
            enemy[4] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;

        }
        else if(enemy[4] == 0 && enemy[3] == 1 && laser_y < 155)
        {
            LCD_DrawFillRectangle(14, 140, 14+14, 160+10, BLACK);
            enemy[3] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
        else if(enemy[3] == 0 && enemy[2] == 1 && laser_y < 140)
        {
            LCD_DrawFillRectangle(14, 125, 14+14, 160+10, BLACK);
            enemy[2] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
        else if(enemy[2] == 0 && enemy[1] == 1 && laser_y < 125)
        {
            LCD_DrawFillRectangle(14, 110, 14+14, 160+10, BLACK);
            enemy[1] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
        else if(enemy[1] == 0 && enemy[0] == 1 && laser_y < 110)
        {
            LCD_DrawFillRectangle(14, 95, 14+14, 160+10, BLACK);
            enemy[0] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
    }
    //enemy2
    else if(last_player_pos < 41 && last_player_pos > 29 && laser_y < 175)
    {
        if(enemy2[4] == 1)
        {
            LCD_DrawFillRectangle(34, 160, 34+14, 160+10, BLACK);
            enemy2[4] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
        else if(enemy2[4] == 0 && enemy2[3] == 1 && laser_y < 155)
        {
            LCD_DrawFillRectangle(34, 140, 34+14, 160+10, BLACK);
            enemy2[3] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
        else if(enemy2[3] == 0 && enemy2[2] == 1 && laser_y < 140)
        {
            LCD_DrawFillRectangle(34, 125, 34+14, 160+10, BLACK);
            enemy2[2] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
        else if(enemy2[2] == 0 && enemy2[1] == 1 && laser_y < 125)
        {
            LCD_DrawFillRectangle(34, 110, 34+14, 160+10, BLACK);
            enemy2[1] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
        else if(enemy2[1] == 0 && enemy2[0] == 1 && laser_y < 110)
        {
            LCD_DrawFillRectangle(34, 95, 34+14, 160+10, BLACK);
            enemy2[0] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
    }
    //enemy3
    else if(last_player_pos < 61 && last_player_pos > 49 && laser_y < 175)
    {
        if(enemy3[4] == 1)
        {
            LCD_DrawFillRectangle(54, 160, 54+14, 160+10, BLACK);
            enemy3[4] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
        else if(enemy3[4] == 0 && enemy3[3] == 1 && laser_y < 155)
        {
            LCD_DrawFillRectangle(54, 140, 54+14, 160+10, BLACK);
            enemy3[3] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
        else if(enemy3[3] == 0 && enemy3[2] == 1 && laser_y < 140)
        {
            LCD_DrawFillRectangle(54, 125, 54+14, 160+10, BLACK);
            enemy3[2] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
        else if(enemy3[2] == 0 && enemy3[1] == 1 && laser_y < 125)
        {
            LCD_DrawFillRectangle(54, 110, 54+14, 160+10, BLACK);
            enemy3[1] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
        else if(enemy3[1] == 0 && enemy3[0] == 1 && laser_y < 110)
        {
            LCD_DrawFillRectangle(54, 95, 54+14, 160+10, BLACK);
            enemy3[0] = 0;
            laser_y = 40;
            shot_fired = 0;
            time += 3;
            score += 10;
        }
    }

    //enemy4
        else if(last_player_pos < 81 && last_player_pos > 69 && laser_y < 175)
        {
            if(enemy4[4] == 1)
            {
                LCD_DrawFillRectangle(74, 160, 74+14, 160+10, BLACK);
                enemy4[4] = 0;
                laser_y = 40;
                shot_fired = 0;
                time += 3;
                score += 10;
            }
            else if(enemy4[4] == 0 && enemy4[3] == 1 && laser_y < 155)
            {
                LCD_DrawFillRectangle(74, 140, 74+14, 160+10, BLACK);
                enemy4[3] = 0;
                laser_y = 40;
                shot_fired = 0;
                time += 3;
                score += 10;
            }
            else if(enemy4[3] == 0 && enemy4[2] == 1 && laser_y < 140)
            {
                LCD_DrawFillRectangle(74, 125, 74+14, 160+10, BLACK);
                enemy4[2] = 0;
                laser_y = 40;
                shot_fired = 0;
                time += 3;
                score += 10;
            }
            else if(enemy4[2] == 0 && enemy4[1] == 1 && laser_y < 125)
            {
                LCD_DrawFillRectangle(74, 110, 74+14, 160+10, BLACK);
                enemy4[1] = 0;
                laser_y = 40;
                shot_fired = 0;
                time += 3;
                score += 10;
            }
            else if(enemy4[1] == 0 && enemy4[0] == 1 && laser_y < 110)
            {
                LCD_DrawFillRectangle(74, 95, 74+14, 160+10, BLACK);
                enemy4[0] = 0;
                laser_y = 40;
                shot_fired = 0;
                time += 3;
                score += 10;
            }
        }

    //enemy5
            else if(last_player_pos < 101 && last_player_pos > 89 && laser_y < 175)
            {
                if(enemy5[4] == 1)
                {
                    LCD_DrawFillRectangle(94, 160, 94+14, 160+10, BLACK);
                    enemy5[4] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy5[4] == 0 && enemy5[3] == 1 && laser_y < 155)
                {
                    LCD_DrawFillRectangle(94, 140, 94+14, 160+10, BLACK);
                    enemy5[3] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy5[3] == 0 && enemy5[2] == 1 && laser_y < 140)
                {
                    LCD_DrawFillRectangle(94, 125, 94+14, 160+10, BLACK);
                    enemy5[2] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy5[2] == 0 && enemy5[1] == 1 && laser_y < 125)
                {
                    LCD_DrawFillRectangle(94, 110, 94+14, 160+10, BLACK);
                    enemy5[1] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy5[1] == 0 && enemy5[0] == 1 && laser_y < 110)
                {
                    LCD_DrawFillRectangle(94, 95, 94+14, 160+10, BLACK);
                    enemy5[0] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
            }


    //enemy6
            else if(last_player_pos < 121 && last_player_pos > 109 && laser_y < 175)
            {
                if(enemy6[4] == 1)
                {
                    LCD_DrawFillRectangle(114, 160, 114+14, 160+10, BLACK);
                    enemy6[4] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy6[4] == 0 && enemy6[3] == 1 && laser_y < 155)
                {
                    LCD_DrawFillRectangle(114, 140, 114+14, 160+10, BLACK);
                    enemy6[3] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy6[3] == 0 && enemy6[2] == 1 && laser_y < 140)
                {
                    LCD_DrawFillRectangle(114, 125, 114+14, 160+10, BLACK);
                    enemy6[2] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy6[2] == 0 && enemy6[1] == 1 && laser_y < 125)
                {
                    LCD_DrawFillRectangle(114, 110, 114+14, 160+10, BLACK);
                    enemy6[1] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy6[1] == 0 && enemy6[0] == 1 && laser_y < 110)
                {
                    LCD_DrawFillRectangle(114, 95, 114+14, 160+10, BLACK);
                    enemy6[0] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
            } //enemy7
            else if(last_player_pos < 141 && last_player_pos > 129 && laser_y < 175)
            {
                if(enemy7[4] == 1)
                {
                    LCD_DrawFillRectangle(134, 160, 134+14, 160+10, BLACK);
                    enemy7[4] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy7[4] == 0 && enemy7[3] == 1 && laser_y < 155)
                {
                    LCD_DrawFillRectangle(134, 140, 134+14, 160+10, BLACK);
                    enemy7[3] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy7[3] == 0 && enemy7[2] == 1 && laser_y < 140)
                {
                    LCD_DrawFillRectangle(134, 125, 134+14, 160+10, BLACK);
                    enemy7[2] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy7[2] == 0 && enemy7[1] == 1 && laser_y < 125)
                {
                    LCD_DrawFillRectangle(134, 110, 134+14, 160+10, BLACK);
                    enemy7[1] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy7[1] == 0 && enemy7[0] == 1 && laser_y < 110)
                {
                    LCD_DrawFillRectangle(134, 95, 134+14, 160+10, BLACK);
                    enemy7[0] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
            }//enemy8
            else if(last_player_pos < 161 && last_player_pos > 149 && laser_y < 175)
            {
                if(enemy8[4] == 1)
                {
                    LCD_DrawFillRectangle(154, 160, 154+14, 160+10, BLACK);
                    enemy8[4] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy8[4] == 0 && enemy8[3] == 1 && laser_y < 155)
                {
                    LCD_DrawFillRectangle(154, 140, 154+14, 160+10, BLACK);
                    enemy8[3] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy8[3] == 0 && enemy8[2] == 1 && laser_y < 140)
                {
                    LCD_DrawFillRectangle(154, 125, 154+14, 160+10, BLACK);
                    enemy8[2] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy8[2] == 0 && enemy8[1] == 1 && laser_y < 125)
                {
                    LCD_DrawFillRectangle(154, 110, 154+14, 160+10, BLACK);
                    enemy8[1] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy8[1] == 0 && enemy8[0] == 1 && laser_y < 110)
                {
                    LCD_DrawFillRectangle(154, 95, 154+14, 160+10, BLACK);
                    enemy8[0] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
            }//enemy8
            else if(last_player_pos < 181 && last_player_pos > 169 && laser_y < 175)
            {
                if(enemy9[4] == 1)
                {
                    LCD_DrawFillRectangle(174, 160, 174+14, 160+10, BLACK);
                    enemy9[4] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy9[4] == 0 && enemy9[3] == 1 && laser_y < 155)
                {
                    LCD_DrawFillRectangle(174, 140, 174+14, 160+10, BLACK);
                    enemy9[3] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy9[3] == 0 && enemy9[2] == 1 && laser_y < 140)
                {
                    LCD_DrawFillRectangle(174, 125, 174+14, 160+10, BLACK);
                    enemy9[2] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy9[2] == 0 && enemy9[1] == 1 && laser_y < 125)
                {
                    LCD_DrawFillRectangle(174, 110, 174+14, 160+10, BLACK);
                    enemy9[1] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy9[1] == 0 && enemy9[0] == 1 && laser_y < 110)
                {
                    LCD_DrawFillRectangle(174, 95, 174+14, 160+10, BLACK);
                    enemy9[0] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
            }//enemy10
            else if(last_player_pos < 201 && last_player_pos > 189 && laser_y < 175)
            {
                if(enemy10[4] == 1)
                {
                    LCD_DrawFillRectangle(194, 160, 194+14, 160+10, BLACK);
                    enemy10[4] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy10[4] == 0 && enemy10[3] == 1 && laser_y < 155)
                {
                    LCD_DrawFillRectangle(194, 140, 194+14, 160+10, BLACK);
                    enemy10[3] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy10[3] == 0 && enemy10[2] == 1 && laser_y < 140)
                {
                    LCD_DrawFillRectangle(194, 125, 194+14, 160+10, BLACK);
                    enemy10[2] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy10[2] == 0 && enemy10[1] == 1 && laser_y < 125)
                {
                    LCD_DrawFillRectangle(194, 110, 194+14, 160+10, BLACK);
                    enemy10[1] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy10[1] == 0 && enemy10[0] == 1 && laser_y < 110)
                {
                    LCD_DrawFillRectangle(194, 95, 194+14, 160+10, BLACK);
                    enemy10[0] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
            }//enemy10
            else if(last_player_pos < 221 && last_player_pos > 209 && laser_y < 175)
            {
                if(enemy11[4] == 1)
                {
                    LCD_DrawFillRectangle(214, 160, 214+14, 160+10, BLACK);
                    enemy11[4] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy11[4] == 0 && enemy11[3] == 1 && laser_y < 155)
                {
                    LCD_DrawFillRectangle(214, 140, 214+14, 160+10, BLACK);
                    enemy11[3] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy11[3] == 0 && enemy11[2] == 1 && laser_y < 140)
                {
                    LCD_DrawFillRectangle(214, 125, 214+14, 160+10, BLACK);
                    enemy11[2] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy11[2] == 0 && enemy11[1] == 1 && laser_y < 125)
                {
                    LCD_DrawFillRectangle(214, 110, 214+14, 160+10, BLACK);
                    enemy11[1] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
                else if(enemy11[1] == 0 && enemy11[0] == 1 && laser_y < 110)
                {
                    LCD_DrawFillRectangle(214, 95, 214+14, 160+10, BLACK);
                    enemy11[0] = 0;
                    laser_y = 40;
                    shot_fired = 0;
                    time += 3;
                    score += 10;
                }
            }
}

//void destroy_enemy(int row, int column, int x_position, int y_position) {
//    LCD_DrawFillRectangle(x_position, y_position, x_position+14, y_position+10, BLACK);
//    enemies[row][column] = 0;
//    laser_y = 40;
//    shot_fired = 0;
//}

//void shot_hit_2()
//{
//    int row = 0;
//    int x = 0;
//    //int column = 0;
//
//    if (last_player_pos > 9 && last_player_pos < 21) {
//        while (enemies[row][0] == 0 && row < 5) {
//            row += 1;
//            laser_y -= 20;
//            x += 20;
//
//        }
//        if (laser_y < 175 - x) {
//            destroy_enemy(4-row, 0, x_positions[0], y_positions[row]);
//            //row = 0;
//        }
//    }
////    else if (last_player_pos > 49 && last_player_pos < 86 && laser_y < 155) {
////        while (enemies[row][1] == 0 && row < 5) {
////                    row += 1;
////        }
////        destroy_enemy(5-row, 1, x_positions[1], y_positions[row]);
////        row = 0;
////    }
//}

void init_DAC() {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER |= 3<<(2*4);

    RCC->APB1ENR |= RCC_APB1ENR_DACEN;
    DAC->CR &= ~DAC_CR_EN1;
    DAC->CR &= ~DAC_CR_BOFF1;
    DAC->CR |= DAC_CR_TEN1;
    DAC->CR |= DAC_CR_TSEL1;
    DAC->CR |= DAC_CR_EN1;
}

// GRAPHICS BELOW
int __io_putchar(int c) {
    if(c == '\n')
    {
        while(!(USART5->ISR & USART_ISR_TXE)) { }
        USART5->TDR = '\r';
    }
    while(!(USART5->ISR & USART_ISR_TXE)) { }
    USART5->TDR = c;
    return c;
}

int __io_getchar(void) {
     return interrupt_getchar();
}

int interrupt_getchar(){

        // Wait for a newline to complete the buffer.
        while(fifo_newline(&input_fifo) == 0) {

            asm volatile ("wfi");
        }
        // Return a character from the line buffer.
        char ch = fifo_remove(&input_fifo);
        return ch;
}

void USART3_4_5_6_7_8_IRQHandler(void) {
            while(DMA2_Channel2->CNDTR != sizeof serfifo - seroffset) {
                if (!fifo_full(&input_fifo))
                    insert_echo_char(serfifo[seroffset]);
                seroffset = (seroffset + 1) % sizeof serfifo;
            }
        }
void init_spi1_slow(){
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    //GPIOB->MODER &= ~(GPIO_MODER_MODER3);
    GPIOB->MODER |= GPIO_MODER_MODER3_1;

    //GPIOB->MODER &= ~(GPIO_MODER_MODER4);
    GPIOB->MODER |= GPIO_MODER_MODER4_1;

    //GPIOB->MODER &= ~(GPIO_MODER_MODER5);
    GPIOB->MODER |= GPIO_MODER_MODER5_1;

    GPIOB->AFR[0] &= ~(GPIO_AFRL_AFR3 | GPIO_AFRL_AFR4 | GPIO_AFRL_AFR5);
    //SPI1->CR1 &= ~(SPI_CR1_SPE);
    SPI1->CR1 |= SPI_CR1_BR;   //baud rate
    SPI1->CR2 &= ~SPI_CR2_DS;   //word size 8 bits 0111
    SPI1->CR1 |= SPI_CR1_MSTR;
    SPI1->CR1 |= SPI_CR1_SSM;
    SPI1->CR1 |= SPI_CR1_SSI;   //internal slave select, dont know if should be enabled
    SPI1->CR2 |= SPI_CR2_FRXTH;
    SPI1->CR1 |= SPI_CR1_SPE;

}
void enable_sdcard(){
    GPIOB->BSRR |= GPIO_BSRR_BR_2;

}
void disable_sdcard(){

    GPIOB->BSRR |= GPIO_BSRR_BS_2;
}
void init_sdcard_io(){
    init_spi1_slow();
    GPIOB->MODER &= ~(GPIO_MODER_MODER2);
    GPIOB->MODER |= GPIO_MODER_MODER2_0;
    disable_sdcard();
}
void sdcard_io_high_speed(){
    SPI1->CR1 &= ~(SPI_CR1_SPE);
    SPI1->CR1 &=  ~(SPI_CR1_BR);
    SPI1->CR1 |= SPI_CR1_BR_0;   //baud rate 001
    SPI1->CR1 |= SPI_CR1_SPE;
}
void init_lcd_spi(){
    //RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    //GPIOB->MODER &= ~(GPIO_MODER_MODER8);
    GPIOB->MODER |= GPIO_MODER_MODER8_0;

    //GPIOB->MODER &= ~(GPIO_MODER_MODER11);
    GPIOB->MODER |= GPIO_MODER_MODER11_0;

    //GPIOB->MODER &= ~(GPIO_MODER_MODER14);
    GPIOB->MODER |= GPIO_MODER_MODER14_0;
    init_spi1_slow();
    sdcard_io_high_speed();
}

//BUTTONS
void init_PC()
{
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC->MODER &= ~GPIO_MODER_MODER0;
    GPIOC->MODER &= ~GPIO_MODER_MODER1;
    GPIOC->MODER &= ~GPIO_MODER_MODER2;
    GPIOC->PUPDR |= GPIO_PUPDR_PUPDR0_1;
    GPIOC->PUPDR |= GPIO_PUPDR_PUPDR1_1;
    GPIOC->PUPDR |= GPIO_PUPDR_PUPDR2_1;
}

int main() {

    //init_usart5();
    //enable_tty_interrupt();
    init_wavetable();
    init_DAC();
    init_TIM6();
    init_TIM2(1500);
    init_TIM7();
    init_TIM15();
    init_PC();

    mp = midi_init(midifile);

    //GRAPHICS BELOW
    init_lcd_spi();
    LCD_Setup();
    LCD_DrawPicture(0, 0, (const Picture *)&gimp_image);





    for(;;) {
        asm("wfi");
    }
}
