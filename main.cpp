#include "functions.h"
#include "LCD_DISCO_F429ZI.h"
#include "DebouncedInterrupt.h"
#include "mbed.h"
#include <time.h>
#include <vector>

#define SDA_PIN PC_9
#define SCL_PIN PA_8
#define AI1_ENABLED 1
#define AI2_ENABLED 1

// DEVICES --------------------------------

LCD_DISCO_F429ZI LCD;
I2C i2c(SDA_PIN, SCL_PIN);
DigitalOut led(PG_14);
Ticker game_ticker;

// INTERRUPTS -----------------------------

InterruptIn onboard_button(BUTTON1);
DebouncedInterrupt external_button1(PA_5);
DebouncedInterrupt external_button2(PA_6);
DebouncedInterrupt external_button3(PA_7);

// DATA TYPES -----------------------------

typedef enum {
  STATE_MENU = 0,
  STATE_PAUSE = 1,
  STATE_GAME = 2,
} State_Type;

// GLOBAL VARS ----------------------------

static State_Type curr_state;

// OBJECTS --------------------------------
// BOARD OBJECT METHODS
        
// Constructor
Board::Board(int min_width, int min_height, int max_width, int max_height) : min_width(min_width), min_height(min_height), max_width(max_width), max_height(max_height) {
    balls.emplace_back(min_width+(max_width-min_width)/2, min_height+(max_height-min_height)/2);
    paddles.emplace_back((int)((float)(max_width-min_width) / 2 - (0.15 * (max_width-min_width)) / 2), min_height + 5, *this);
    paddles.emplace_back((int)((float)(max_width-min_width) / 2 - (0.15 * (max_width-min_width)) / 2), max_height - 10, *this);
    score1 = 0;
    score2 = 0;
}

// Destructor
Board::~Board() {}

int Board::getMinHeight() const {return min_height;}
int Board::getMinWidth() const {return min_width;}
int Board::getMaxHeight() const {return max_height;}
int Board::getMaxWidth() const {return max_width;}
void Board::drawBalls() {
    for (int i = 0; i < balls.size(); i++) {
        balls[i].draw();
    }
}
void Board::moveBalls() {
    for (int i = 0; i < balls.size(); i++) {
        bool del = false;
        balls[i].move(*this, del);
        if (del) {
            balls.erase(balls.begin() + i);
            i--;
        }
    }
    if (balls.size() <= 0) {balls.emplace_back(min_width+(max_width-min_width)/2, min_height+(max_height-min_height)/2);}
    
    // AI opponent
    if (balls[0].getx() < paddles[1].getLeft() && AI1_ENABLED) {
        paddles[1].moveLeft();
    } else if (balls[0].getx() > paddles[1].getRight() && AI1_ENABLED) {
        paddles[1].moveRight();
    }
    if (balls[0].getx() < paddles[0].getLeft() && AI2_ENABLED) {
        paddles[0].moveLeft();
    } else if (balls[0].getx() > paddles[0].getRight() && AI2_ENABLED) {
        paddles[0].moveRight();
    }
}
void Board::incrementScore1() {score1++;}
void Board::incrementScore2() {score2++;}
int Board::getScore1() const {return score1;}
int Board::getScore2() const {return score2;}

// BALL OBJECT METHODS

Ball::Ball(int x, int y) : x(x), y(y) {
    radius = 3;
    x_speed = 1;
    y_speed = -1;
}
Ball::~Ball() {}

void Ball::draw() {
    // Code to draw the ball at position (x, y)
    LCD.SetTextColor(LCD_COLOR_WHITE);
    LCD.FillCircle(x, y, radius);
}
int Ball::getx() {return x;}
void Ball::move(Board& board, bool& del) {
    x = x + x_speed;
    y = y + y_speed;
    del = false;
    if (y-radius <= board.getMinHeight()) {
        board.incrementScore2();
        del = true;
    } else if (y+radius >= board.getMaxHeight()) {
        board.incrementScore1();
        del = true;
    } else if (x-radius <= board.getMinWidth()) {
        x_speed = abs(x_speed);
        x = abs(x-board.getMinWidth()) + board.getMinWidth();
        x = max(board.getMinWidth()+radius, x);
    } else if (x+radius >= board.getMaxWidth()) {
        x_speed = -abs(x_speed);
        x = board.getMaxWidth() - abs(x-board.getMaxWidth());
        x = min(board.getMaxWidth()-radius, x);
    }

    if (y-radius <= board.paddles[0].getBottom() && x <= board.paddles[0].getRight() && x >= board.paddles[0].getLeft()) {
        y_speed = abs(y_speed);
    } else if (y+radius >= board.paddles[1].getTop() && x <= board.paddles[1].getRight() && x >= board.paddles[1].getLeft()) {
        y_speed = -abs(y_speed);
    }
}
// PADDLE OBJECT METHODS

Paddle::Paddle(int x, int y, Board& board) : x(x), y(y), board(board) {
    height = 5;
    width = 0.15 * (board.getMaxWidth()-board.getMinWidth());
    }
Paddle::~Paddle() {}

int Paddle::getLeft() {
    return x;
}
int Paddle::getRight() {
    return x+width;
}
int Paddle::getTop() {
    return y;
}
int Paddle::getBottom() {
    return y+height;
}
void Paddle::draw() {
    // Code to draw the paddle at position (x, y)
    LCD.SetTextColor(LCD_COLOR_WHITE);
    LCD.FillRect(x, y, width, height);
}
void Paddle::moveRight() {
    if (x+width <= board.getMaxWidth()) {
        x = x + 0.25*width;
        x = min(x, board.getMaxWidth()-width);
    }
}
void Paddle::moveLeft() {
    if (x > board.getMinWidth()) {
        x = x - 0.25*width;
        x = max(board.getMinWidth(), x);
    }
}

Board board(0, 20, 240, 320);

// ISRs -----------------------------------

void ExternalButton1ISR() {
    board.paddles[0].moveLeft();
}

void ExternalButton2ISR() {
    if (curr_state == STATE_GAME) {
        curr_state = STATE_PAUSE;
    } else if (curr_state == STATE_PAUSE) {
        curr_state = STATE_GAME;
    }
}

void ExternalButton3ISR() {
    board.paddles[0].moveRight();
}

void OnboardButtonISR() {
    if (curr_state == STATE_MENU) {
        curr_state = STATE_GAME;
    } else if (curr_state == STATE_PAUSE) {
        curr_state = STATE_MENU;
    } 
}

void TickerISR() {
    board.moveBalls();
}

// FSM SET UP ------------------------------

void stateMenu(void);
void statePause(void);
void stateGame();

static void (*state_table[])(void) = {stateMenu, statePause, stateGame};

void initializeSM() {
  curr_state = STATE_MENU;
}

// HELPER FUNCTIONS ------------------------



// STATE FUNCTIONS ---------------------------

void stateMenu() {
    LCD.Clear(LCD_COLOR_BLUE);
}

void statePause() {
    LCD.Clear(LCD_COLOR_RED);
}

void stateGame() {
    LCD.Clear(LCD_COLOR_BLACK);
    
    // Draw the scoreboard
    LCD.SetTextColor(LCD_COLOR_WHITE);
    LCD.FillRect(board.getMaxWidth()-board.getMinWidth(), 0, board.getMaxWidth()-board.getMinWidth(), board.getMinHeight());
    LCD.SetTextColor(LCD_COLOR_BLACK);
    LCD.SetFont(&Font12);
    char score_str[30];
    int score1 = board.getScore1();
    int score2 = board.getScore2();
    if (score1 > score2) {
        sprintf(score_str, "SCORE: %d - %d (P1)", score1, score2);
    } else if (score2 > score1) {
        sprintf(score_str, "SCORE: %d - %d (P2)", score1, score2);
    } else {
        sprintf(score_str, "SCORE: %d - %d", score1, score2);
    }
    LCD.DisplayStringAt(0, board.getMinHeight()/2-4, (uint8_t *)score_str, CENTER_MODE);

    // Draw the board and paddles
    board.drawBalls();
    board.paddles[0].draw();
    board.paddles[1].draw();
}

// MAIN FUNCTION -----------------------------

int main() {
    game_ticker.attach(&TickerISR, 20ms);
    onboard_button.fall(&OnboardButtonISR);
    external_button1.attach(&ExternalButton1ISR, IRQ_FALL, 50, false);
    external_button2.attach(&ExternalButton2ISR, IRQ_FALL, 50, false);
    external_button3.attach(&ExternalButton3ISR, IRQ_FALL, 50, false);
    initializeSM();
    while (1) {
        state_table[curr_state]();
        thread_sleep_for(20);
    }
}