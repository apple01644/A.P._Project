#include <iostream>
#include <fstream>
#include <vector>
#include <time.h>
#include <random>
#define _USE_MATH_DEFINES
#include <math.h>

#include <opencv2/opencv.hpp>

using namespace std;

enum class BlockType {
	None, Block, Ball
};

enum class State {
	Prepare,
	Shoot,
	Shooting,
	CleanUp,
	Result
};

struct Block {
	BlockType type = BlockType::None;
	int num = 0;
	int lastHitId = -1;
	uint64_t lastHitTime = -1;
	Block() {}
	Block(const BlockType& type, const int& num) : type(type), num(num) {

	}
};

struct Ball {
	float x, y, vx, vy;
	int id;
};

struct Rect {
	float x, y, w, h;
};

struct _2Point {
	float x1, y1, x2, y2;
	_2Point(float x1, float y1, float x2, float y2) : x1(x1), y1(y1), x2(x2), y2(y2) {}
};

template<typename T>
struct Point {
	T x, y;
};

struct Circle {
	float x, y, r;
};

bool Collision_Rect_Circle(const Rect& rect, const Circle& circle)
{
	_2Point rectA(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
	_2Point rectB(circle.x - circle.r, circle.y - circle.r, circle.x + circle.r, circle.y + circle.r);

	if (rectA.x1 < rectB.x2 && rectA.x2 > rectB.x1 && rectA.y1 < rectB.y2 && rectA.y2 > rectB.y1)
	{
		if ((circle.y > rect.y + rect.h || circle.y < rect.y) && (circle.x > rect.x + rect.w || circle.x < rect.x))
		{
			Point<float> near;

			if (circle.x > rect.x + rect.w / 2)
				near.x = rect.x + rect.w;
			else
				near.x = rect.x;

			if (circle.y > rect.y + rect.h / 2)
				near.y = rect.y + rect.h;
			else
				near.y = rect.y;

			return sqrtf(powf(near.x - circle.x, 2) + powf(near.y - circle.y, 2)) <= circle.r;
		}
		else
		{
			return true;
		}
	}
	else
	{
		return false;
	}
}

class SBBGame {
public:
	const static int Game_Width = 6;
	const static int Game_Height = 9;
	Block Game_Map[Game_Width][Game_Height];

	std::vector<Ball> Game_shootBalls;
	int Game_Score;
	float Game_X, Game_NX;
	const float Game_Y = 100 - ball_rad;
	int Game_LeftBalls, Game_Balls;
	State Game_State;
	uint64_t Game_LastShoot = 0;
	uint64_t Game_t = 0, attempt = 0;
	float Game_ShootRad;
	bool Game_groundBall = false;

	bool Flag_user = false;
	bool Flag_custom_state = false;
	bool Flag_run = true;

	const float dt = 0.1f;
	const float ball_rad = 100.f / Game_Height * 0.185f;

	mt19937_64 mt = mt19937_64(time(nullptr));

	SBBGame() {
		initialize();
	}

	void action_prepare()
	{
		int cursor = mt() % Game_Width;
		Game_Map[cursor][0] = Block(BlockType::Ball, 1);

		while (true)
		{
			cursor = mt() % Game_Width;
			if (Game_Map[cursor][0].type == BlockType::None)
			{
				Game_Map[cursor][0] = Block(BlockType::Block, Game_Score);
				break;
			}
		}

		int left_ball = Game_Score / 20 + 1;
		for (int x = 0; x < Game_Width; ++x)
		{
			Block& block = Game_Map[x][0];
			if (block.type == BlockType::None)
			{
				if (mt() % 2 == 0)
				{
					Game_Map[cursor][0] = Block(BlockType::Block, Game_Score);
					if (--left_ball == 0) break;
				}
			}
			
		}

		for (int _y = 0; _y < Game_Height - 1; ++_y)
		{
			int y = Game_Height - 1 - 1 - _y;
			for (int x = 0; x < Game_Width; ++x)
			{
				Block& block = Game_Map[x][y];
				if (block.type != BlockType::None)
				{
					Game_Map[x][y + 1] = block;
					block = Block(BlockType::None, 0);
				}
			}
		}

		for (int x = 0; x < Game_Width; ++x)
		{
			Block& block = Game_Map[x][0];
			switch (block.type)
			{
			case BlockType::Block:
				//Game Over
				cout << "Score is " << Game_Score << endl;
				++attempt;
				initialize();
				return;
			case BlockType::Ball:
				++Game_Balls;
				block = Block(BlockType::None, 0);
				break;
			}
		}

		Game_State = State::Shoot;
	}

	void action_cleanup()
	{
		Game_X = Game_NX;
		Game_State = State::Prepare;
		Game_t = 0;
	}
	void action_shoot(const float& deg)
	{
		float rad = (deg * ((float)M_PI - 0.3f)) - (float)M_PI + 0.15f;
		Game_LeftBalls = Game_Balls;
		Game_ShootRad = rad;
		Game_LastShoot = Game_t;
		Game_groundBall = false;
		Game_State = State::Shooting;
	}

	void initialize()
	{
		for (int x = 0; x < Game_Width; ++x)
			for (int y = 0; y < Game_Height; ++y)
			{
				Game_Map[x][y] = Block(BlockType::None, 0);
			}


		Game_Score = 1;
		Game_Balls = 1;
		Game_X = 50;
		
		Game_State = State::Prepare;
	}

	void loop() {
		++Game_t;
		switch (Game_State)
		{
		case State::Shooting:
			if (Game_LeftBalls > 0)
			{
				if (Game_t - Game_LastShoot > 30)
				{
					Game_shootBalls.push_back(Ball{ Game_X, Game_Y, cosf(Game_ShootRad), sinf(Game_ShootRad), Game_LeftBalls });
					--Game_LeftBalls;
					Game_LastShoot = Game_t;
				}
			}
			else
			{
				if (Game_shootBalls.size() == 0)
				{
					++Game_Score;
					if (Flag_user)
						Game_State = State::CleanUp;
					else
						Game_State = State::Result;					
				}
			}
			break;
		case State::Prepare:
			if (!Flag_custom_state)
				action_prepare();
			break;
		case State::CleanUp:
			action_cleanup();
			break;
		}

		for (Ball& ball : Game_shootBalls)
		{
			ball.x -= ball.vx * dt;
			ball.y -= ball.vy * dt;

			if (ball.x > 100 - ball_rad)
			{
				ball.x = 100 - ball_rad;
				ball.vx = -fabsf(ball.vx);
			}
			if (ball.x < ball_rad)
			{
				ball.x = ball_rad;
				ball.vx = fabsf(ball.vx);
			}
			if (ball.y < ball_rad)
			{
				ball.y = ball_rad;
				ball.vy = fabsf(ball.vy);
			}

			for (int x = 0; x < Game_Width; ++x)
				for (int y = 0; y < Game_Height; ++y)
				{
					Block& block = Game_Map[x][y];
					switch (block.type)
					{
					case BlockType::Block:
						if (Collision_Rect_Circle(Rect{ x * 100 / 6.f, y * 100 / 9.f, 100 / 6.f, 100 / 9.f }, Circle{ ball.x, ball.y, ball_rad }))
						{
							if ((ball.y > (y * 100 / 9.f + 1) + (100 / 9.f - 2) || ball.y < (y * 100 / 9.f + 1)) &&
								(ball.x > (x * 100 / 6.f + 1) + (100 / 6.f - 2) || ball.x < (x * 100 / 6.f + 1)))
							{
								Point<float> near;
								Point<int> point;

								if (ball.x > (x * 100 / 6.f + 1) + (100 / 6.f - 2) / 2)
								{
									near.x = (x * 100 / 6.f + 1) + (100 / 6.f - 2);
									point.x = 1;
								}
								else
								{
									near.x = (x * 100 / 6.f + 1);
									point.x = 0;
								}

								if (ball.y > (y * 100 / 9.f + 1) + (100 / 9.f - 2) / 2)
								{
									near.y = (y * 100 / 9.f + 1) + (100 / 9.f - 2);
									point.y = 1;
								}
								else
								{
									near.y = (y * 100 / 9.f + 1);
									point.y = 0;
								}


								bool passed[2][2];

								passed[0][0] = ((0 <= x - 1) && (x - 1 < 6) && (Game_Map[x - 1][y].type != BlockType::Block));
								passed[0][1] = ((0 <= x + 1) && (x + 1 < 6) && (Game_Map[x + 1][y].type != BlockType::Block));

								passed[1][0] = ((0 <= y - 1) && (y - 1 < 9) && (Game_Map[x][y - 1].type != BlockType::Block));
								passed[1][1] = ((0 <= y + 1) && (y + 1 < 9) && (Game_Map[x][y + 1].type != BlockType::Block));

								if (passed[0][point.x] && passed[1][point.y])
								{
									Point<float>vec{ near.x - ball.x, near.y - ball.y };
									float rad_C2P = atan2f(vec.y, vec.x) + (float)M_PI_2;
									float rad = atan2f(ball.vx, ball.vy);
									float length = sqrtf(powf(ball.vy, 2) + powf(ball.vx, 2));
									float new_rad = -(rad - rad_C2P) + rad_C2P;

									while (new_rad < (float)M_PI)
										new_rad += (float)M_PI * 2;
									new_rad -= (float)M_PI * 2;

									if (new_rad >= 0.f && new_rad < 0.15f)
									{
										new_rad = 0.15f;
									}
									if (new_rad >= -0.15f && new_rad < 0.f)
									{
										new_rad = -0.15f;
									}
									if (new_rad >= M_PI - 0.15f && new_rad < M_PI)
									{
										new_rad = (float)M_PI - 0.15f;
									}
									if (new_rad >= -M_PI && new_rad < 0.15 - M_PI)
									{
										new_rad = 0.15f - (float)M_PI;
									}

									ball.vx = cosf(new_rad);
									ball.vy = sinf(new_rad);
								}
								else
								{
									int side = -1;//East, North, West, South
									switch (point.x)
									{
									case 1:
										switch (point.y)
										{
										case 1:
											if (!passed[0][1]) side = 3;
											else if (!passed[1][1]) side = 0;
											break;
										case 0:
											if (!passed[0][1]) side = 1;
											else if (!passed[1][0]) side = 0;
											break;
										}
										break;
									case 0:
										switch (point.y)
										{
										case 1:
											if (!passed[0][0]) side = 3;
											else if (!passed[1][1]) side = 2;
											break;
										case 0:
											if (!passed[0][0]) side = 1;
											else if (!passed[1][0]) side = 2;
											break;
										}
										break;
									}

									switch (side)
									{
									case 0:
										ball.vx = fabsf(ball.vx);
										break;
									case 1:
										ball.vy = -fabsf(ball.vy);
										break;
									case 2:
										ball.vx = -fabsf(ball.vx);
										break;
									case 3:
										ball.vy = fabsf(ball.vy);
										break;
									}
								}
							}
							else
							{
								Point<float> vec{ (ball.x - (x + 0.5f) * 100 / 6) * 6, (ball.y - (y + 0.5f) * 100 / 9) * 9 };
								float rad = atan2f(vec.y, vec.x) / (float)M_PI;
								if (rad >= 1 / 4 && rad < 3 / 4)
									ball.vy = fabsf(ball.vy);
								else if (rad >= 3 / 4 && rad < -3 / 4)
									ball.vx = -fabsf(ball.vx);
								else if (rad >= -3 / 4 && rad < -1 / 4)
									ball.vy = -fabsf(ball.vy);
								else if (rad >= -1 / 4 && rad < 1 / 4)
									ball.vx = fabsf(ball.vx);
							}

							if (block.lastHitId != ball.id || block.lastHitTime + dt * 15 < Game_t)
							{
								--block.num;
								//Data Log
								block.lastHitId = ball.id;
								block.lastHitTime = Game_t;
							}
						}
						break;
					case BlockType::Ball:
						{
							Point<float> vec{ ball.x - 100 / 6.f * (x + 0.5f), ball.y - 100 / 9.f * (y + 0.5f) };
							if (sqrtf(powf(vec.x, 2) + powf(vec.y, 2)) <= ball_rad * 2 && block.num > 0)
							{
								block.num = 0;
								//Data Log
							}
						}
						break;
					}
				}
		}

		for (int x = 0; x < Game_Width; ++x)
			for (int y = 0; y < Game_Height; ++y)
			{
				Block& block = Game_Map[x][y];
				switch (block.type)
				{
				case BlockType::Block:
					if (block.num <= 0) 
					{
						block.type = BlockType::None;
						cout << "Break Block\n";
					}
					break;
				case BlockType::Ball:
					if (block.num <= 0)
					{
						block.type = BlockType::None;
						cout << "Eat Ball\n";
						++Game_Balls;
					}
					break;
				}
			}

		vector<Ball> balls;
		balls.swap(Game_shootBalls);

		for (Ball& ball : balls)
		{
			if (ball.y < 100 - ball_rad)
			{
				Game_shootBalls.push_back(ball);
			}
			else if (!Game_groundBall)
			{
				Game_groundBall = true;
				Game_NX = ball.x;
			}
		}
	}
};

int main()
{
	cv::Mat wind(640, 480, CV_8UC3);
	cv::imshow("mainWindow", wind);

	SBBGame game;
	while (game.Flag_run)
	{
		switch (game.Game_State)
		{
		case State::Shoot:
			game.action_shoot(0.5f);
			break;
		case State::Result:
			cout << "one turn end" << endl;
			game.Game_State = State::Prepare;
			game.Game_t = 0;
			break;
		}
		game.loop();
	}

	return 0;
}