#include "iGraphics.h"

#include <windows.h>   // SHIFT detection
#include <mmsystem.h>  // mciSendString
#pragma comment(lib, "winmm.lib")

#include <math.h>
#include <stdlib.h>    // rand, srand
#include <time.h>      // time

// ================= WINDOW =================
const int WIN_W = 600;
const int WIN_H = 400;

// ================= IMAGES =================
int backgroundImage;
int backgroundImage2;
int charImg[6];
int zombieImg[7];

// ================= MOUSE / VISION =================
int mouseX = 0, mouseY = 0;
double viewAngle = 0.0;     // radians
double fov = 0.9;           // ~ 50 degrees
int visionLength = 220;
int rayCount = 35;

// ================= PLAYER =================
int playerX = 200;
int playerY = 200;
int playerSize = 30;

int playerHP = 100;
bool gameOver = false;

// animation
int charFrame = 0;
int charFrameSpeed = 5;
int charFrameCounter = 0;

// ================= RUN / SOUND =================
bool isRunning = false;
int soundRadius = 0;

// ================= ZOMBIES =================
const int ZOMBIE_COUNT = 50;
int zombiesSpawned = 0;

double zombieX[ZOMBIE_COUNT];
double zombieY[ZOMBIE_COUNT];
int zombieSize[ZOMBIE_COUNT];
int zombieSpeed[ZOMBIE_COUNT];
int zombieHP[ZOMBIE_COUNT];
bool zombieAlive[ZOMBIE_COUNT];

// ================= ZOMBIE AI =================
enum ZState { Z_WANDER, Z_INVESTIGATE };

ZState zState[ZOMBIE_COUNT];

double wanderTX[ZOMBIE_COUNT];   // target x for wandering
double wanderTY[ZOMBIE_COUNT];   // target y for wandering

int wanderTimer[ZOMBIE_COUNT];   // when timer ends, pick a new target

double hearX[ZOMBIE_COUNT];      // investigation target x
double hearY[ZOMBIE_COUNT];      // investigation target y
int hearTimer[ZOMBIE_COUNT];     // how long to keep investigating


// ================= SOUND SYSTEM =================
int walkRadius = 50;     // small
int runRadius = 120;    // bigger
int shootRadius = 180;   // biggest (pulse)

int currentSoundRadius = 0;

// shooting pulse lasts for short time
int shootPulseTimer = 0;          // counts down in update ticks
const int SHOOT_PULSE_DURATION = 12; // ~ 12*20ms = 240ms if fixedUpdate every 20ms


// ================= BULLETS =================
const int MAX_BULLETS = 10;

int bulletX[MAX_BULLETS];
int bulletY[MAX_BULLETS];
bool bulletActive[MAX_BULLETS];

double bulletDX[MAX_BULLETS];
double bulletDY[MAX_BULLETS];

int bulletSpeed = 10;

// ================= AMMO / SCORE =================
int ammo = 10;
int maxAmmo = 10;
int score = 0;


// ================= SCREEN / MENU =================
enum Screen { SCREEN_MENU, SCREEN_GAME, SCREEN_SETTINGS, SCREEN_CREDITS };
Screen currentScreen = SCREEN_MENU;

// Simple button struct
struct Button {
	int x, y, w, h;
	const char* text;
};

Button btnPlay = { 220, 240, 160, 40, "PLAY" };
Button btnSettings = { 220, 190, 160, 40, "SETTINGS" };
Button btnCredits = { 220, 140, 160, 40, "CREDITS" };
Button btnQuit = { 220, 90, 160, 40, "QUIT" };

bool isInside(int mx, int my, Button b)
{
	return (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h);
}

void drawButton(Button b)
{
	iSetColor(40, 40, 40);
	iFilledRectangle(b.x, b.y, b.w, b.h);

	iSetColor(255, 255, 255);
	iRectangle(b.x, b.y, b.w, b.h);

	iText(b.x + 55, b.y + 12, (char*)b.text);
}




// =====================================================
// SPAWN ZOMBIE
// =====================================================
void spawnZombie()
{
	if (zombiesSpawned >= ZOMBIE_COUNT) return;

	int i = zombiesSpawned;

	// spawn from edges
	int edge = rand() % 4;
	switch (edge)
	{
	case 0: zombieX[i] = 0;     zombieY[i] = rand() % WIN_H; break;
	case 1: zombieX[i] = WIN_W; zombieY[i] = rand() % WIN_H; break;
	case 2: zombieX[i] = rand() % WIN_W; zombieY[i] = 0;     break;
	case 3: zombieX[i] = rand() % WIN_W; zombieY[i] = WIN_H; break;
	}

	zombieSize[i] = 30;
	zombieSpeed[i] = (i < 3) ? 1 : 2;
	zombieHP[i] = 20;
	zombieAlive[i] = true;

	zState[i] = Z_WANDER;
	wanderTimer[i] = 0;
	hearTimer[i] = 0;


	zombiesSpawned++;
}

// Reset the entire game state (used for restart and starting a new game)
void resetGame()
{
    playerX = 200;
    playerY = 200;
    playerHP = 100;
    score = 0;
    ammo = maxAmmo;
    gameOver = false;

    currentScreen = SCREEN_GAME;
    isRunning = false;
    shootPulseTimer = 0;
    currentSoundRadius = 0;
    charFrame = 0;
    charFrameCounter = 0;

    // reset zombies
    zombiesSpawned = 0;
    for (int i = 0; i < ZOMBIE_COUNT; i++)
    {
        zombieAlive[i] = false;
        zombieHP[i] = 20;
        zombieX[i] = 0;
        zombieY[i] = 0;
        zombieSize[i] = 30;
        zombieSpeed[i] = (i < 3) ? 1 : 2;
        zState[i] = Z_WANDER;
        wanderTimer[i] = 0;
        hearTimer[i] = 0;
        hearX[i] = 0;
        hearY[i] = 0;
    }

    // reset bullets
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        bulletActive[i] = false;
        bulletX[i] = 0;
        bulletY[i] = 0;
        bulletDX[i] = 0.0;
        bulletDY[i] = 0.0;
    }

    // spawn initial zombie
    spawnZombie();

    mciSendString("stop ggsong", NULL, 0, NULL);
    mciSendString("play bgsong repeat", NULL, 0, NULL);
}

// =====================================================
// DRAW
// =====================================================

void drawDarkness()
{
	int cx = playerX + playerSize / 2;
	int cy = playerY + playerSize / 2;

	for (int x = 0; x < WIN_W; x += 1)
	{
		for (int y = 0; y < WIN_H; y += 4)
		{
			double dx = x - cx;
			double dy = y - cy;

			double dist = sqrt(dx * dx + dy * dy);
			double angleToPoint = atan2(dy, dx);

			double angleDiff = fabs(angleToPoint - viewAngle);

			if (angleDiff > 3.1416)
				angleDiff = 2 * 3.1416 - angleDiff;

			bool insideCone =
				(dist < visionLength) &&
				(angleDiff < fov / 2.0);

			if (!insideCone)
			{
				iShowImage(0, 0, 0, 0 , backgroundImage2);
			}
		}
	}
}


void drawMenu()
{
	iSetColor(61, 12, 2);
	iFilledRectangle(0, 0, WIN_W, WIN_H);

	iSetColor(255, 255, 255);
	iText(180, 330, "Echos Of The Undying", GLUT_BITMAP_TIMES_ROMAN_24);
	iText(210, 300, "Best of Luck", GLUT_BITMAP_HELVETICA_18);

	drawButton(btnPlay);
	drawButton(btnSettings);
	drawButton(btnCredits);
	drawButton(btnQuit);

	iText(145, 30, "Mouse: Aim | WASD: Move | Shift+WASD: Run | Click: Shoot");
}

void drawSettings()
{
	iSetColor(0, 0, 0);
	iFilledRectangle(0, 0, WIN_W, WIN_H);

	iSetColor(255, 255, 255);
	iText(250, 300, "SETTINGS", GLUT_BITMAP_TIMES_ROMAN_24);
	iText(210, 240, "Coming soon...", GLUT_BITMAP_HELVETICA_18);
	iText(170, 60, "Press B to go Back", GLUT_BITMAP_HELVETICA_18);
}

void drawCredits()
{
	iSetColor(0, 0, 0);
	iFilledRectangle(0, 0, WIN_W, WIN_H);

	iSetColor(255, 255, 255);
	iText(260, 300, "CREDITS", GLUT_BITMAP_TIMES_ROMAN_24);
	iText(190, 240, "Made by: Prince,Sakib,Rishad", GLUT_BITMAP_HELVETICA_18);
	iText(170, 60, "Press B to go Back", GLUT_BITMAP_HELVETICA_18);
}



void iDraw()
{
	iClear();

	if (currentScreen == SCREEN_MENU) { drawMenu(); return; }
	if (currentScreen == SCREEN_SETTINGS) { drawSettings(); return; }
	if (currentScreen == SCREEN_CREDITS) { drawCredits(); return; }

	// if we reach here, we are in GAME
	// (keep your existing game drawing code below)

	iShowImage(0, 0, WIN_W, WIN_H, backgroundImage);

	// movement check (for animation only)
	bool moving =
		isKeyPressed('w') || isKeyPressed('a') || isKeyPressed('s') || isKeyPressed('d') ||
		isSpecialKeyPressed(GLUT_KEY_UP) || isSpecialKeyPressed(GLUT_KEY_DOWN) ||
		isSpecialKeyPressed(GLUT_KEY_LEFT) || isSpecialKeyPressed(GLUT_KEY_RIGHT);

	if (moving && !gameOver)
	{
		charFrameCounter++;
		if (charFrameCounter >= charFrameSpeed)
		{
			charFrameCounter = 0;
			charFrame = (charFrame + 1) % 6;
		}
	}
	else
	{
		charFrame = 0;
	}

	// player sprite (draw ONCE) - rotated to face mouse cursor
	double viewAngleDegrees = viewAngle * (180.0 / 3.14159265359) - 90.0;
	glPushMatrix();
	glTranslatef(playerX + playerSize / 2.0f, playerY + playerSize / 2.0f, 0);
	glRotatef(viewAngleDegrees, 0, 0, 1);
	glTranslatef(-(playerSize / 2.0f), -(playerSize / 2.0f), 0);
	iShowImage(0, 0, playerSize, playerSize, charImg[charFrame]);
	glPopMatrix();

	// ===== flashlight rays =====
	int frontOffset = 15;
	int cx = playerX + playerSize / 2 + (int)(cos(viewAngle) * frontOffset);
	int cy = playerY + playerSize / 2 + (int)(sin(viewAngle) * frontOffset);

	for (int i = -rayCount; i <= rayCount; i++)
	{
		double t = (double)i / (double)rayCount;
		double ang = viewAngle + (t * (fov / 2.0));

		int x2 = cx + (int)(cos(ang) * visionLength);
		int y2 = cy + (int)(sin(ang) * visionLength);

		iSetColor(200, 200, 200);
		iLine(cx, cy, x2, y2);
	}

	// center ray
	int xMid = cx + (int)(cos(viewAngle) * visionLength);
	int yMid = cy + (int)(sin(viewAngle) * visionLength);
	iSetColor(255, 255, 255);
	iLine(cx, cy, xMid, yMid);

	// Dark overlay
	drawDarkness();


	// zombies
	for (int i = 0; i < zombiesSpawned; i++)
	{
		if (!zombieAlive[i]) continue;
		int imgIndex = i % 7;
		iShowImage((int)zombieX[i], (int)zombieY[i], zombieSize[i], zombieSize[i], zombieImg[imgIndex]);
	}

	// bullets
	iSetColor(255, 255, 0);
	for (int i = 0; i < MAX_BULLETS; i++)
	{
		if (bulletActive[i])
			iFilledCircle(bulletX[i], bulletY[i], 4);
	}

	// HUD
	iSetColor(255, 255, 255);
	iText(20, 20, "Echos Of The Undying");

	char hpText[50];
	sprintf(hpText, "HP: %d", playerHP);
	iText(20, 80, hpText);

	char scoreText[50];
	sprintf(scoreText, "SCORE: %d", score);
	iText(20, 50, scoreText);

	char ammoText[50];
	sprintf(ammoText, "AMMO: %d", ammo);
	iText(20, 110, ammoText);

	// sound circle
	if (isRunning && !gameOver)
	{
		iSetColor(255, 0, 0);
		iCircle(playerX + playerSize / 2, playerY + playerSize / 2, soundRadius);
	}

	// game over text
	if (gameOver)
	{
		iSetColor(255, 0, 0);
		iText(180, 230, "GAME OVER", GLUT_BITMAP_TIMES_ROMAN_24);
		iText(220, 200, "Press R to Restart", GLUT_BITMAP_HELVETICA_18);
	}
}

// =====================================================
// MOUSE
// =====================================================
void iMouseMove(int mx, int my)
{
	mouseX = mx;
	mouseY = my;

	int cx = playerX + playerSize / 2;
	int cy = playerY + playerSize / 2;

	viewAngle = atan2((double)(mouseY - cy), (double)(mouseX - cx));
}

void iPassiveMouseMove(int mx, int my)
{
	iMouseMove(mx, my);
}

void iMouse(int button, int state, int mx, int my)
{
	// ================= MENU CLICK =================
	if (currentScreen == SCREEN_MENU &&
		button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		if (isInside(mx, my, btnPlay))
		{
			// start a fresh game when Play is clicked
			resetGame();
		}
		else if (isInside(mx, my, btnSettings))
		{
			currentScreen = SCREEN_SETTINGS;
		}
		else if (isInside(mx, my, btnCredits))
		{
			currentScreen = SCREEN_CREDITS;
		}
		else if (isInside(mx, my, btnQuit))
		{
			mciSendString("stop bgsong", NULL, 0, NULL);
			mciSendString("stop ggsong", NULL, 0, NULL);
			exit(0);
		}
		return;
	}

	// ================= GAME MOUSE (SHOOT) =================
	if (currentScreen == SCREEN_GAME &&
		button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && !gameOver)
	{
		if (ammo <= 0) return;
		ammo--;

		// 🔊 Shooting sound pulse (big radius for a short time)
		shootPulseTimer = SHOOT_PULSE_DURATION;

		for (int i = 0; i < MAX_BULLETS; i++)
		{
			if (!bulletActive[i])
			{
				bulletActive[i] = true;
				bulletX[i] = playerX + playerSize / 2;
				bulletY[i] = playerY + playerSize / 2;

				bulletDX[i] = cos(viewAngle);
				bulletDY[i] = sin(viewAngle);
				break;
			}
		}
		return;
	}
}


double dist2(double ax, double ay, double bx, double by)
{
	double dx = ax - bx;
	double dy = ay - by;
	return dx*dx + dy*dy;
}

void makeSoundEvent(int sx, int sy, int radius)
{
	int r2 = radius * radius;

	for (int i = 0; i < zombiesSpawned; i++)
	{
		if (!zombieAlive[i]) continue;

		if (dist2(zombieX[i], zombieY[i], sx, sy) <= r2)
		{
			zState[i] = Z_INVESTIGATE;
			hearX[i] = sx;
			hearY[i] = sy;
			hearTimer[i] = 80; // how long they stay interested (~1.6s if 20ms update)
		}
	}
}

void moveZombieToward(int i, double tx, double ty)
{
	double dx = tx - zombieX[i];
	double dy = ty - zombieY[i];

	double d = sqrt(dx*dx + dy*dy);
	if (d > 0.001)
	{
		zombieX[i] += (dx / d) * zombieSpeed[i];
		zombieY[i] += (dy / d) * zombieSpeed[i];
	}
}

void updateZombieAI()
{
	for (int i = 0; i < zombiesSpawned; i++)
	{
		if (!zombieAlive[i]) continue;

		if (zState[i] == Z_INVESTIGATE)
		{
			// move to last heard position
			moveZombieToward(i, hearX[i], hearY[i]);

			// reduce interest timer
			hearTimer[i]--;
			if (hearTimer[i] <= 0)
			{
				zState[i] = Z_WANDER;
				wanderTimer[i] = 0; // force new wander target
			}
		}
		else // Z_WANDER
		{
			wanderTimer[i]--;
			if (wanderTimer[i] <= 0)
			{
				// pick random point in the map
				wanderTX[i] = rand() % WIN_W;
				wanderTY[i] = rand() % WIN_H;
				wanderTimer[i] = 100 + (rand() % 120); // wander for a bit
			}

			moveZombieToward(i, wanderTX[i], wanderTY[i]);
		}
	}
}



// =====================================================
// UPDATE
// =====================================================
void fixedUpdate()
{
    if (currentScreen != SCREEN_GAME)
    {
        // allow returning from Settings/Credits by polling key (some environments
        // may not deliver iKeyboard events reliably). This makes 'B' work
        // even when not in the game loop.
        if (currentScreen == SCREEN_SETTINGS || currentScreen == SCREEN_CREDITS)
        {
            if (isKeyPressed('b') || isKeyPressed('B'))
            {
                currentScreen = SCREEN_MENU;
                return;
            }
        }

        return;
    }

    // If game over, still poll for restart key so keyboard can restart the game
    if (gameOver)
    {
        if (isKeyPressed('r') || isKeyPressed('R'))
            resetGame();
        return;
    }

	// ----- STRICT CONTROLS -----
	bool shiftHeld = (GetKeyState(VK_LSHIFT) & 0x8000) || (GetKeyState(VK_RSHIFT) & 0x8000);
	bool wWalk = (!shiftHeld) && isKeyPressed('w');
	bool aWalk = (!shiftHeld) && isKeyPressed('a');
	bool sWalk = (!shiftHeld) && isKeyPressed('s');
	bool dWalk = (!shiftHeld) && isKeyPressed('d');

	bool wRun = (shiftHeld) && isKeyPressed('w');
	bool aRun = (shiftHeld) && isKeyPressed('a');
	bool sRun = (shiftHeld) && isKeyPressed('s');
	bool dRun = (shiftHeld) && isKeyPressed('d');

	bool up = isSpecialKeyPressed(GLUT_KEY_UP);
	bool down = isSpecialKeyPressed(GLUT_KEY_DOWN);
	bool left = isSpecialKeyPressed(GLUT_KEY_LEFT);
	bool right = isSpecialKeyPressed(GLUT_KEY_RIGHT);

	int walkSpeed = 1;
	int runSpeed = 3;

	// WALK
	if (wWalk) playerY += walkSpeed;
	if (sWalk) playerY -= walkSpeed;
	if (aWalk) playerX -= walkSpeed;
	if (dWalk) playerX += walkSpeed;

	// RUN
	if (wRun) playerY += runSpeed;
	if (sRun) playerY -= runSpeed;
	if (aRun) playerX -= runSpeed;
	if (dRun) playerX += runSpeed;

	// ARROWS (walk)
	if (up) playerY += walkSpeed;
	if (down) playerY -= walkSpeed;
	if (left) playerX -= walkSpeed;
	if (right) playerX += walkSpeed;

	// running state
	isRunning = (wRun || aRun || sRun || dRun);

	// --------------------------------
	// 🔊 SOUND SYSTEM
	// --------------------------------

	bool walkingNow = (wWalk || aWalk || sWalk || dWalk || up || down || left || right);

	currentSoundRadius = 0;

	if (walkingNow)
		currentSoundRadius = walkRadius;

	if (isRunning)
		currentSoundRadius = runRadius;

	if (currentSoundRadius > 0)
	{
		makeSoundEvent(playerX + playerSize / 2,
			playerY + playerSize / 2,
			currentSoundRadius);
	}

	// Shooting pulse
	if (shootPulseTimer > 0)
	{
		shootPulseTimer--;
		makeSoundEvent(playerX + playerSize / 2,
			playerY + playerSize / 2,
			shootRadius);
	}

	// --------------------------------

	// reload
	if (isKeyPressed('q')) ammo = maxAmmo;

	// keep inside window
	if (playerX < 0) playerX = 0;
	if (playerY < 0) playerY = 0;
	if (playerX + playerSize > WIN_W) playerX = WIN_W - playerSize;
	if (playerY + playerSize > WIN_H) playerY = WIN_H - playerSize;

	score++;

	// --------------------------------
	// BULLETS
	// --------------------------------

	for (int i = 0; i < MAX_BULLETS; i++)
	{
		if (!bulletActive[i]) continue;

		bulletX[i] += (int)(bulletDX[i] * bulletSpeed);
		bulletY[i] += (int)(bulletDY[i] * bulletSpeed);

		if (bulletX[i] < 0 || bulletX[i] > WIN_W ||
			bulletY[i] < 0 || bulletY[i] > WIN_H)
			bulletActive[i] = false;
	}

	// Bullet -> Zombie
	for (int i = 0; i < MAX_BULLETS; i++)
	{
		if (!bulletActive[i]) continue;

		for (int j = 0; j < zombiesSpawned; j++)
		{
			if (!zombieAlive[j]) continue;

			bool hitX = bulletX[i] > zombieX[j] &&
				bulletX[i] < zombieX[j] + zombieSize[j];
			bool hitY = bulletY[i] > zombieY[j] &&
				bulletY[i] < zombieY[j] + zombieSize[j];

			if (hitX && hitY)
			{
				zombieHP[j] -= 50;
				bulletActive[i] = false;

				if (zombieHP[j] <= 0)
				{
					zombieAlive[j] = false;
					score += 10;
				}
				break;
			}
		}
	}

	// --------------------------------
	// 🧠 ZOMBIE AI UPDATE
	// --------------------------------

	updateZombieAI();

	// --------------------------------
	// ZOMBIE DAMAGE
	// --------------------------------

	for (int i = 0; i < zombiesSpawned; i++)
	{
		if (!zombieAlive[i]) continue;

		bool collisionX =
			playerX < zombieX[i] + zombieSize[i] &&
			playerX + playerSize > zombieX[i];

		bool collisionY =
			playerY < zombieY[i] + zombieSize[i] &&
			playerY + playerSize > zombieY[i];

		if (collisionX && collisionY)
		{
			playerHP--;

			if (playerHP <= 0)
			{
				gameOver = true;
				mciSendString("stop bgsong", NULL, 0, NULL);
				mciSendString("play ggsong", NULL, 0, NULL);
			}
		}
	}
}


// =====================================================
// KEYBOARD (RESTART)
// =====================================================
void iKeyboard(unsigned char key)
{
    // Allow returning from Settings screen by pressing B
    if ((key == 'b' || key == 'B') && currentScreen == SCREEN_SETTINGS)
    {
        currentScreen = SCREEN_MENU;
        return;
    }

	if (key == 'r' || key == 'R')
	{
		if (gameOver)
		{
			// use centralized reset
			resetGame();
		}
	}
}

// =====================================================
// MAIN
// =====================================================
int main()
{
	SetProcessDPIAware();

	srand((unsigned)time(NULL));

	// audio
	mciSendString("open \"Audios//background.mp3\" alias bgsong", NULL, 0, NULL);
	mciSendString("open \"Audios//gameover.mp3\" alias ggsong", NULL, 0, NULL);
	mciSendString("play bgsong repeat", NULL, 0, NULL);

	// init arrays
	for (int i = 0; i < MAX_BULLETS; i++) bulletActive[i] = false;
	for (int i = 0; i < ZOMBIE_COUNT; i++) zombieAlive[i] = false;

	iInitialize(WIN_W, WIN_H, "Hospital Horror Game");

	// load images
	backgroundImage = iLoadImage("Images\\bg1.png");
	backgroundImage2 = iLoadImage("Images\\bg2.png");

	charImg[0] = iLoadImage("Images\\run1.png");
	charImg[1] = iLoadImage("Images\\run2.png");
	charImg[2] = iLoadImage("Images\\run3.png");
	charImg[3] = iLoadImage("Images\\run4.png");
	charImg[4] = iLoadImage("Images\\run5.png");
	charImg[5] = iLoadImage("Images\\run6.png");

	zombieImg[0] = iLoadImage("Images\\zombie1.png");
	zombieImg[1] = iLoadImage("Images\\zombie2.png");
	zombieImg[2] = iLoadImage("Images\\zombie3.png");
	zombieImg[3] = iLoadImage("Images\\zombie4.png");
	zombieImg[4] = iLoadImage("Images\\zombie5.png");
	zombieImg[5] = iLoadImage("Images\\zombie6.png");
	zombieImg[6] = iLoadImage("Images\\zombie7.png");

	// timers
	iSetTimer(1000, spawnZombie); // every 1 sec
	iSetTimer(10, fixedUpdate);   // ~50 FPS

	iStart();
	return 0;
}
