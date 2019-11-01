#include <iostream>
#include <fstream>
#include <sstream>

#include <vector>
#include <unordered_map>
#include <time.h>
#include <random>

#include <thread>
#include <future>
#include <functional>
#include <algorithm>

#include <opencv2/opencv.hpp>

#define _AMD64_
#include <processthreadsapi.h>

#include <DirectXMath.h>

#include <ppl.h>

using namespace std;

mt19937_64 mt = mt19937_64(time(nullptr));
const unsigned short&& random()
{
	return mt();
}

enum class BlockType {
	None, Block, Ball
};

enum class State {
	Prepare,
	Shoot,
	Shooting,
	CleanUp,
	Result,
	Die,
	Animation
};

enum class AnimationState {
	None,
	Dying,
	GettingDowns
};

enum class GameState {
	Login,
	Play,
	GameOver
};

struct Block {
	BlockType type = BlockType::None;
	int num = 0;
	unordered_map<int, uint32_t> lastHit;
	uint32_t lastHitTime;
	Block() {}
	Block(const BlockType& type, const int& num) : type(type), num(num) {

	}
};

struct Ball {
	float x, y, vx, vy;
	int id;
	bool hitted = false;
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

struct Log {
	int x;
	int y;
	int t;
	BlockType type;
};

bool Collision_Rect_Circle(const Rect& rect, const Circle& circle)
{
	_2Point rectA(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
	_2Point rectB(circle.x - circle.r, circle.y - circle.r, circle.x + circle.r, circle.y + circle.r);

	if (rectA.x1 < rectB.x2 && rectA.x2 > rectB.x1 && rectA.y1 < rectB.y2 && rectA.y2 > rectB.y1)
	{
		if ((circle.y > rect.y + rect.h || circle.y < rect.y) && (circle.x > rect.x + rect.w || circle.x < rect.x))
		{
			Point<float> nearest;

			if (circle.x > rect.x + rect.w / 2)
				nearest.x = rect.x + rect.w;
			else
				nearest.x = rect.x;

			if (circle.y > rect.y + rect.h / 2)
				nearest.y = rect.y + rect.h;
			else
				nearest.y = rect.y;

			return sqrtf(powf(nearest.x - circle.x, 2) + powf(nearest.y - circle.y, 2)) <= circle.r;
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

DirectX::XMFLOAT3 blend(const DirectX::XMFLOAT3 highColor, const DirectX::XMFLOAT3 lowColor, float value)
{
	return DirectX::XMFLOAT3{
		highColor.x* value + lowColor.x * (1 - value),
		highColor.y* value + lowColor.y * (1 - value),
		highColor.z* value + lowColor.z * (1 - value)
	};
}

const static int Game_Width = 6;
const static int Game_Height = 9;

struct GameAbstractData {
	float shoot_x;
	float type_map[Game_Width][Game_Height];
	float num_map[Game_Width][Game_Height];
};

struct Effect {
	float x, y, size, rad;
	DirectX::XMFLOAT3 color;
};

array<float, Game_Width> create_BoardHeader(const int Game_Score)
{
	array<float, Game_Width> ret;
	for (float& e : ret) e = 0.f;

	int cursor = random() % Game_Width;
	ret[cursor] = -1.f;

	while (true)
	{
		cursor = random() % Game_Width;
		if (ret[cursor] == 0.f)
		{
			ret[cursor] = 1.f;
			break;
		}
	}

	int left_ball = 5;
	for (int x = 0; x < Game_Width; ++x)
	{
		if (ret[x] == 0.f)
		{
			if (random() % 100 < 50 + Game_Score)
			{
				ret[x] = 1.f;
				if (--left_ball == 0) break;
			}
		}
	}

	return ret;
}

class SBBGame {
public:
	Block Game_Map[Game_Width][Game_Height];

	std::vector<Ball> Game_shootBalls;
	int Game_Score;
	float Game_X, Game_NX = 50;
	int Game_LeftBalls, Game_Balls;
	State Game_State;

	AnimationState Game_Animation = AnimationState::None;
	float Game_Animation_Progress;
	float Game_Animation_Dx = 0.f;
	float Game_Animation_Dy = 0.f;

	uint64_t Game_LastShoot = 0;
	uint64_t Game_t = 0, attempt = 0;
	float Game_ShootRad;
	bool Game_groundBall = false;

	bool Flag_user = false;
	bool Flag_custom_state = false;
	bool Flag_effect = false;
	bool Flag_run = true;
	bool Flag_animation = false;
	bool Flag_roofOff = false;
	bool Flag_under_clear = false;

	bool Penalty_disable_BlockNumber = false;
	bool Penalty_disable_GuideLine = false;

	const float dt = 0.1f;
	const float ball_rad = 100.f / Game_Height * 0.185f;
	const float Game_Y = 100 - ball_rad;

	std::list<Log> logs;

	cv::HersheyFonts font_face = cv::HersheyFonts::FONT_HERSHEY_DUPLEX;
	double font_size = 0.0025 * 480;
	int font_thick = 3;

	float display_DeltaX = 0;
	float display_DeltaY = 0;
	float display_Size = 640;
	float round_width = 0.1f;

	DirectX::XMFLOAT3 highest_color = { 230.f,   5.f,   0.f };
	DirectX::XMFLOAT3  lowest_color = { 253.f, 204.f,  77.f };
	DirectX::XMFLOAT3  green_ball_color = { 0.f, 255.f,  0.f };
	DirectX::XMFLOAT3  blue_ball_color = { 94.f, 167.f,  243.f }; 
	const DirectX::XMFLOAT3  hitted_color = { 255.f, 255.f, 255.f };

	DirectX::XMFLOAT3  back_color = { 200.f, 200.f, 200.f };
	DirectX::XMFLOAT3  fore_color = { 255.f, 255.f, 255.f };

	bool guide_enable = false;
	float guide_line;

	std::list<Effect> Game_effects;
	

	SBBGame() {
		initialize();
	}

	void action_prepare()
	{
		const auto data = create_BoardHeader(Game_Score);
		action_prepare(data);
	}

	void action_prepare(const array<float, Game_Width> data)
	{
		for (int x = 0; x < data.size(); ++x)
		{
			if (data[x] == -1.f)
			{
				Game_Map[x][0] = Block(BlockType::Ball, 1);
			}
			else if(data[x] == 1.f)
			{
				Game_Map[x][0] = Block(BlockType::Block, Game_Score);
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
					block.lastHit.clear();
					Game_Map[x][y + 1] = block;
					block = Block(BlockType::None, 0);
				}
			}
		}

		for (int x = 0; x < Game_Width; ++x)
		{
			Block& block = Game_Map[x][Game_Height - 1];
			switch (block.type)
			{
			case BlockType::Block:
				//Game Over
				cout << "Score is " << Game_Score << endl;
				++attempt;
				if (Flag_animation)
				{
					Game_State = State::Animation;
					Game_Animation = AnimationState::Dying;
					Game_Animation_Progress = 0.f;
				}
				else
				{
					Game_State = State::Die;
				}
				return;
			case BlockType::Ball:
				if (!Flag_animation)
				{
					++Game_Balls;
					block = Block(BlockType::None, 0);
				}
				break;
			}
		}

		++Game_Score;
		if (Flag_animation)
		{
			Game_State = State::Animation;
			Game_Animation = AnimationState::GettingDowns;
			Game_Animation_Progress = 0.f;
			Game_Animation_Dy -= (int)display_Size / 9;
		}
		else
		{
			Game_State = State::Shoot;
		}
		log_begin();
	}

	void action_cleanup()
	{
		Game_X = Game_NX;
		Game_State = State::Prepare;
		Game_t = 0;
	}
	void action_shoot(const float& deg)
	{
		float rad = (deg * (DirectX::XM_PI - 0.3f)) - DirectX::XM_PI + 0.15f;
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

	void custom_initialize(const GameAbstractData& data, int balls, int score) {
		
		importData(data, balls);
		Game_Score = score;
		Game_State = State::Shoot;
	}

	void loop() {	
		++Game_t;
		switch (Game_State)
		{
		case State::Shooting:
			if (Game_LeftBalls > 0)
			{
				if (Game_t - Game_LastShoot > 100)
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
			ball.x += ball.vx * dt;
			ball.y += ball.vy * dt;

			if (ball.x > 100 - ball_rad)
			{
				ball.x = 100 - ball_rad;
				ball.vx = -abs(ball.vx);
			}
			if (ball.x < ball_rad)
			{
				ball.x = ball_rad;
				ball.vx = abs(ball.vx);
			}
			if (ball.y < ball_rad)
			{
				ball.y = ball_rad;
				ball.vy = abs(ball.vy);
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
							if (const auto itr = block.lastHit.find(ball.id);(itr == block.lastHit.end() || itr->second + 1.5f / dt < Game_t) && block.num > 0)
							{
								if ((ball.y > ((y + 1) * 100 / 9.f - round_width) || ball.y < (y * 100 / 9.f + round_width)) &&
									(ball.x > ((x + 1) * 100 / 6.f - round_width) || ball.x < (x * 100 / 6.f + round_width)))
								{
									Point<float> nearest;
									Point<int> point;

									if (ball.x > (x + 1) * 100 / 6.f - round_width)
									{
										nearest.x = (x + 1) * 100 / 6.f - round_width;
										point.x = 1;
									}
									else
									{
										nearest.x = x * 100 / 6.f + round_width;
										point.x = 0;
									}

									if (ball.y > (y + 1) * 100 / 9.f - round_width)
									{
										nearest.y = (y + 1) * 100 / 9.f - round_width;
										point.y = 1;
									}
									else
									{
										nearest.y = y * 100 / 9.f + round_width;
										point.y = 0;
									}


									bool passed[2][2];

									passed[0][0] = ((0 <= x - 1) && (Game_Map[x - 1][y].type != BlockType::Block));
									passed[0][1] = ((x + 1 < 6) && (Game_Map[x + 1][y].type != BlockType::Block));

									passed[1][0] = ((0 <= y - 1) && (Game_Map[x][y - 1].type != BlockType::Block));
									passed[1][1] = ((y + 1 < 9) && (Game_Map[x][y + 1].type != BlockType::Block));

									if (passed[0][point.x] && passed[1][point.y])
									{
										Point<float>vec{ ball.x - nearest.x, ball.y - nearest.y };
										float new_rad = atan2f(vec.y, vec.x);

										/*float rad_C2P = atan2f(vec.y, vec.x) + DirectX::XM_PIDIV2;
										float rad = atan2f(ball.vx, ball.vy);
										float length = sqrtf(powf(ball.vy, 2) + powf(ball.vx, 2));
										float new_rad = -(rad - rad_C2P) + rad_C2P;
										*/
										while (new_rad < DirectX::XM_PI)
											new_rad += DirectX::XM_2PI;
										new_rad -= DirectX::XM_2PI;
										
										if (new_rad >= 0.f && new_rad < 0.15f)
										{
											new_rad = 0.15f;
										}
										if (new_rad >= -0.15f && new_rad < 0.f)
										{
											new_rad = -0.15f;
										}
										if (new_rad >= DirectX::XM_PI - 0.15f && new_rad < DirectX::XM_PI)
										{
											new_rad = DirectX::XM_PI - 0.15f;
										}
										if (new_rad >= -DirectX::XM_PI && new_rad < 0.15 - DirectX::XM_PI)
										{
											new_rad = 0.15f - DirectX::XM_PI;
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
											ball.vx = abs(ball.vx);
											break;
										case 1:
											ball.vy = -abs(ball.vy);
											break;
										case 2:
											ball.vx = -abs(ball.vx);
											break;
										case 3:
											ball.vy = abs(ball.vy);
											break;
										}
									}
								}
								else
								{
									Point<float> vec{ ball.x - (x + 0.5f) * 100 / 6, ball.y - (y + 0.5f) * 100 / 9};
									float rad = atan2f(vec.y, vec.x) / DirectX::XM_PI;
									if (rad >= 1.f / 4 && rad < 3.f / 4)
										ball.vy = abs(ball.vy);
									else if (rad >= 3.f / 4 || rad < -3.f / 4)
										ball.vx = -abs(ball.vx);
									else if (rad >= -3.f / 4 && rad < -1.f / 4)
										ball.vy = -abs(ball.vy);
									else if (rad >= -1.f / 4 || rad < 1.f / 4)
										ball.vx = abs(ball.vx);
								}

								--block.num;
								logs.push_back(Log{ x, y, block.num, BlockType::Block });
								block.lastHit[ball.id] = Game_t;
								block.lastHitTime = Game_t;
								if (block.num == 0)
								{
									if (Flag_effect)
									{
										DirectX::XMFLOAT3 color = blend(highest_color, lowest_color, Game_Map[x][y].num / (float)Game_Score);
										for (int _ = 0; _ < 15; ++_)
										{
											float rad = (_ / 7.5f) * DirectX::XM_2PI;
											Game_effects.push_back(
												Effect{
													100 / 6.f * (x + 0.5f),
													100 / 9.f * (y + 0.5f),
													2.f,
													rad,
													color
												});
										}
									}
								}
							}
							else if (itr != block.lastHit.end()) itr->second = Game_t;
							ball.hitted = true;
						}
						break;
					case BlockType::Ball:
						{
							Point<float> vec{ ball.x - 100 / 6.f * (x + 0.5f), ball.y - 100 / 9.f * (y + 0.5f) };
							if (sqrtf(powf(vec.x, 2) + powf(vec.y, 2)) <= ball_rad * 2 && block.num > 0)
							{
								block.num = 0;
								logs.push_back(Log{x, y, 0, BlockType::Ball});
								if (Flag_effect)
								{
									for (int _ = 0; _ < 15; ++_)
									{
										float rad = (_ / 7.5f) * DirectX::XM_2PI;
										DirectX::XMFLOAT3 color = { 0, 255, 0 };
										Game_effects.push_back(
											Effect{
												100 / 6.f * (x + 0.5f),
												100 / 9.f * (y + 0.5f),
												2.f,
												rad,
												color
											});
									}
								}
							}
						}
						break;
					}
				}
		}

		float max_y = 0.f;
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
					}
					else if (float value = 100.f * (y + 1) / Game_Height; value > max_y)
							max_y = value;		
					break;
				case BlockType::Ball:
					if (block.num <= 0)
					{
						block.type = BlockType::None;
						++Game_Balls;
					}
					else if (float value = 100.f * (y + 1) / Game_Height; value > max_y)
						max_y = value;
					break;
				}
			}

		vector<Ball> balls;
		balls.swap(Game_shootBalls);

		bool clear_balls_by_nohit = false;
		bool clear_balls_by_under = true;

		for (Ball& ball : balls)
		{
			if (ball.y <= Game_Y && (ball.y >= ball_rad * 1.5f || !Flag_roofOff))
			{
				Game_shootBalls.push_back(ball);
				if (ball.vy < 0) clear_balls_by_under = false;
				else if (ball.y < max_y) clear_balls_by_under = false;
			}
			else if (!Game_groundBall)
			{
				if (Flag_roofOff && ball.y < ball_rad * 1.5f)
				{
					if (Game_Balls > 1) --Game_Balls;
				}
				else
				{
					Game_groundBall = true;
					Game_NX = ball.x;
					
					if (Flag_effect && Game_effects.size() < 0xff)
					{
						for (int _ = 0; _ < 15; ++_)
						{
							float rad = (_ / 7.5f) * DirectX::XM_2PI;
							DirectX::XMFLOAT3 color = blue_ball_color;
							Game_effects.push_back(
								Effect{
									ball.x - cosf(rad) * ball_rad,
									ball.y - sinf(rad) * ball_rad,
									2.f,
									rad,
									color
								});
						}
					}

					if (!ball.hitted) clear_balls_by_nohit = true;
				}
			}
			else
				if (Flag_effect && Game_effects.size() < 0xff)
				{
					for (int _ = 0; _ < 15; ++_)
					{
						float rad = (_ / 7.5f) * DirectX::XM_2PI;
						DirectX::XMFLOAT3 color = blue_ball_color;
						Game_effects.push_back(
							Effect{
								ball.x - cosf(rad),
								ball.y - sinf(rad),
								2.f,
								rad,
								color
							});
					}
				}
		}
		if (clear_balls_by_nohit) {
			Game_LeftBalls = 0;
		}
		if (clear_balls_by_under && Game_shootBalls.size() > 0 && Game_LeftBalls == 0 && Flag_under_clear) {
			for (const Ball& ball : Game_shootBalls)
			{
				if (Flag_effect && Game_effects.size() < 0xff)
				{
					for (int _ = 0; _ < 15; ++_)
					{
						float rad = (_ / 7.5f) * DirectX::XM_2PI;
						DirectX::XMFLOAT3 color = blue_ball_color;
						Game_effects.push_back(
							Effect{
								ball.x - cosf(rad),
								ball.y - sinf(rad),
								2.f,
								rad,
								color
							});
					}
				}
			}
			Game_shootBalls.clear();
		}
		if (max_y == 0.f && Game_groundBall) {
			for (const Ball& ball : Game_shootBalls)
			{
				if (Flag_effect)
				{
					for (int _ = 0; _ < 15; ++_)
					{
						float rad = (_ / 7.5f) * DirectX::XM_2PI;
						DirectX::XMFLOAT3 color = blue_ball_color;
						Game_effects.push_back(
							Effect{
								ball.x,
								ball.y,
								2.f,
								rad,
								color
							});
					}
				}
			}
			Game_shootBalls.clear();
		}
	}

	void draw(cv::Mat& mat) {
		mat = cv::Scalar(back_color.z, back_color.y, back_color.x);
		cv::rectangle(mat, cv::Rect(0, 0, display_Size, display_Size), cv::Scalar(fore_color.z, fore_color.y, fore_color.x), cv::FILLED, cv::LINE_AA);

		if (Flag_effect && Game_effects.size())
		{
			std::list<Effect> effects;
			effects.swap(Game_effects);

			for (Effect& effect : effects)
			{
				if (effect.x - effect.size < 100 &&
					effect.y - effect.size < 100 &&
					effect.x + effect.size > 0 &&
					effect.y + effect.size > 0 &&
					effect.size > 0)
				{
					cv::rectangle(mat, cv::Rect((effect.x - effect.size / 2) / 100 * display_Size, (effect.y - effect.size / 2) / 100 * display_Size, effect.size / 100 * display_Size, effect.size / 100 * display_Size), cv::Scalar(effect.color.z, effect.color.y, effect.color.x), cv::FILLED, cv::LINE_AA);

					effect.x += cosf(effect.rad) * effect.size / 2;
					effect.y += sinf(effect.rad) * effect.size / 2;
					//effect.rad += 0.9f;
					effect.size -= 0.09f;
					effect.color.x += 0.03f;
					effect.color.y += 0.03f;
					effect.color.z += 0.03f;
					
					Game_effects.push_back(effect);
				}
			}
		}

		cv::Point animationDelta = cv::Point( Game_Animation_Dx , Game_Animation_Dy );
		if (guide_enable && Game_State == State::Shoot)
		{
			Point<float> pt = { Game_X, Game_Y };
			float rad = (guide_line * (DirectX::XM_PI - 0.3f)) - DirectX::XM_PI + 0.15f;
			float vx = cosf(rad), vy = sinf(rad);

			int time = Penalty_disable_GuideLine ? 32460 : 0;
			list<cv::Point2f> pts;
			while (pt.y <= Game_Y && time < 32768) {
				if (time % 24 == 0) {
					pts.push_back(cv::Point2f(pt.x, pt.y));
				}
				++time;
				pt.x += vx * dt;
				pt.y += vy * dt;
				
			
				if (pt.x > 100 - ball_rad)
				{
					pt.x = 100 - ball_rad;
					vx = -abs(vx);
				}
				if (pt.x < ball_rad)
				{
					pt.x = ball_rad;
					vx = abs(vx);
				}
				if (pt.y < ball_rad)
				{
					pt.y = ball_rad;
					vy = abs(vy);
					if (Flag_roofOff)
					{
						time = 32768;
					}
				}

				for (int x = 0; x < Game_Width; ++x)
					for (int y = 0; y < Game_Height; ++y)
					{
						Block& block = Game_Map[x][y];
						if (block.type== BlockType::Block)
							if (Collision_Rect_Circle(Rect{ x * 100 / 6.f, y * 100 / 9.f, 100 / 6.f, 100 / 9.f }, Circle{ pt.x, pt.y, ball_rad }))
							{				
								if ((pt.y > ((y + 1) * 100 / 9.f - round_width) || pt.y < (y * 100 / 9.f + round_width)) &&
									(pt.x > ((x + 1) * 100 / 6.f - round_width) || pt.x < (x * 100 / 6.f + round_width)))
								{
									Point<float> nearest;
									Point<int> point;

									if (pt.x > (x * 100 / 6.f + 1) + (100 / 6.f - 2) / 2)
									{
										nearest.x = (x * 100 / 6.f + 1) + (100 / 6.f - 2);
										point.x = 1;
									}
									else
									{
										nearest.x = (x * 100 / 6.f + 1);
										point.x = 0;
									}

									if (pt.y > (y * 100 / 9.f + 1) + (100 / 9.f - 2) / 2)
									{
										nearest.y = (y * 100 / 9.f + 1) + (100 / 9.f - 2);
										point.y = 1;
									}
									else
									{
										nearest.y = (y * 100 / 9.f + 1);
										point.y = 0;
									}


									bool passed[2][2];

									passed[0][0] = ((0 <= x - 1) && (Game_Map[x - 1][y].type != BlockType::Block));
									passed[0][1] = ((x + 1 < 6) && (Game_Map[x + 1][y].type != BlockType::Block));
									passed[1][0] = ((0 <= y - 1) && (Game_Map[x][y - 1].type != BlockType::Block));
									passed[1][1] = ((y + 1 < 9) && (Game_Map[x][y + 1].type != BlockType::Block));

									if (passed[0][point.x] && passed[1][point.y])
									{
										Point<float>vec{ nearest.x - pt.x, nearest.y - pt.y };
										float rad_C2P = atan2f(vec.y, vec.x) + DirectX::XM_PIDIV2;
										float rad = atan2f(vx, vy);
										float length = sqrtf(powf(vy, 2) + powf(vx, 2));
										float new_rad = -(rad - rad_C2P) + rad_C2P;

										while (new_rad < DirectX::XM_PI)
											new_rad += DirectX::XM_2PI;
										new_rad -= DirectX::XM_2PI;

										if (new_rad >= 0.f && new_rad < 0.15f)
										{
											new_rad = 0.15f;
										}
										if (new_rad >= -0.15f && new_rad < 0.f)
										{
											new_rad = -0.15f;
										}
										if (new_rad >= DirectX::XM_PI - 0.15f && new_rad < DirectX::XM_PI)
										{
											new_rad = DirectX::XM_PI - 0.15f;
										}
										if (new_rad >= -DirectX::XM_PI && new_rad < 0.15 - DirectX::XM_PI)
										{
											new_rad = 0.15f - DirectX::XM_PI;
										}

										vx = cosf(new_rad);
										vy = sinf(new_rad);
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
											vx = abs(vx);
											break;
										case 1:
											vy = -abs(vy);
											break;
										case 2:
											vx = -abs(vx);
											break;
										case 3:
											vy = abs(vy);
											break;
										}
									}
								}
								else
								{
									Point<float> vec{ pt.x - (x + 0.5f) * 100 / 6, pt.y - (y + 0.5f) * 100 / 9 };
									float rad = atan2f(vec.y, vec.x) / DirectX::XM_PI;
									if (rad >= 1.f / 4 && rad < 3.f / 4)
										vy = abs(vy);
									else if (rad >= 3.f / 4 || rad < -3.f / 4)
										vx = -abs(vx);
									else if (rad >= -3.f / 4 && rad < -1.f / 4)
										vy = -abs(vy);
									else if (rad >= -1.f / 4 || rad < 1.f / 4)
										vx = abs(vx);
								}
								time = 32768;
							}
					}
			}


			if (pts.size() >= 2)
			{
				auto itr_f = pts.cbegin();
				auto itr_b = ++pts.cbegin();
				int x = 0;
				while (itr_b != pts.cend())
				{

					if (++x % 2 == 0)cv::line(mat, (*itr_f) * display_Size / 100.f, (*itr_b) * display_Size / 100.f, cv::Scalar(blue_ball_color.z, blue_ball_color.y, blue_ball_color.x), 3, cv::LINE_AA);
					++itr_f;
					++itr_b;
				}

				if (!Penalty_disable_GuideLine) cv::circle(mat, cv::Point(display_Size* itr_f->x / 100, display_Size * itr_f->y / 100), ball_rad* display_Size / 100, cv::Scalar(blue_ball_color.z, blue_ball_color.y, blue_ball_color.x), cv::FILLED, cv::LINE_AA);
			}

		}

		if (Flag_roofOff)
		{
			float y = 0;
			for (int x = 0; x < 16; ++x)
			{
				cv::line(mat, cv::Point(0, y), cv::Point(display_Size, y) , cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
				y += sqrtf(x) / 2 * (1 + (Game_t % 40) / 1600.f);
			}
		}

		for (int x = 0; x < Game_Width; ++x)
		{
			for (int y = 0; y < Game_Height; ++y)
			{
				switch (Game_Map[x][y].type)
				{
				case BlockType::Block:
					{ 
						DirectX::XMFLOAT3 color = blend(highest_color, lowest_color, Game_Map[x][y].num / (float)Game_Score);
						if (Penalty_disable_BlockNumber)
							color = { 0.5f, 0.5f, 0.5f };
						if (Game_t - Game_Map[x][y].lastHitTime < 60)
						{
							color = blend(color, hitted_color, (Game_t - Game_Map[x][y].lastHitTime) / 120.f + 0.5f);							
						}
						cv::rectangle(mat, cv::Rect(display_Size * x / 6.f + animationDelta.x, display_Size * y / 9.f + animationDelta.y, display_Size / 6.f - 1, display_Size / 9.f - 1), cv::Scalar(color.z, color.y, color.x), cv::FILLED, cv::LINE_AA);
						const cv::String str = Penalty_disable_BlockNumber ? "???" : to_string(Game_Map[x][y].num);
						const cv::Size& size = cv::getTextSize(str, font_face, font_size, font_thick, nullptr);
						cv::putText(mat, str, cv::Point(display_Size * (x + 0.5f) / 6 - size.width / 2, display_Size * (y + 0.5f) / 9 + size.height / 2) + animationDelta, font_face, font_size, cv::Scalar(255, 255, 255), font_thick, cv::LINE_AA);
					}
					break;
				case BlockType::Ball:
					cv::circle(mat, cv::Point(display_Size * (x + 0.5f) / 6, display_Size * (y + 0.5f) / 9) + animationDelta, (ball_rad + 0.625f + 0.3f * (sinf(Game_t / 60.f) / 2 + 0.5f)) * display_Size / 100, cv::Scalar(green_ball_color.z, green_ball_color.y, green_ball_color.x), 2, cv::LINE_AA);
					cv::circle(mat, cv::Point(display_Size * (x + 0.5f) / 6, display_Size * (y + 0.5f) / 9) + animationDelta, ball_rad * display_Size / 100, cv::Scalar(green_ball_color.z, green_ball_color.y, green_ball_color.x), cv::FILLED, cv::LINE_AA);
					break;
				}
			}
		}

		for (const Ball& ball : Game_shootBalls)
		{
			cv::circle(mat, cv::Point(display_Size * ball.x / 100, display_Size * ball.y / 100), ball_rad * display_Size / 100, cv::Scalar(243, 167, 94), cv::FILLED, cv::LINE_AA);
		}
		
		if (Game_State != State::Shooting || Game_LeftBalls > 0)
		{
			const cv::String str = "x" + to_string((Game_State == State::Shooting) ? Game_LeftBalls : Game_Balls);
			const cv::Size& size = cv::getTextSize(str, font_face, font_size * 0.67f, font_thick * 0.67f, nullptr);
			cv::putText(mat, str, cv::Point(Game_X * display_Size / 100 - size.width / 2, display_Size * 8.3f / 9 + size.height / 2), font_face, font_size * 0.67f, cv::Scalar(blue_ball_color.z, blue_ball_color.y, blue_ball_color.x), font_thick * 0.67f, cv::LINE_AA);
			cv::circle(mat, cv::Point(Game_X * display_Size / 100, display_Size - ball_rad * display_Size / 100), ball_rad * display_Size / 100, cv::Scalar(blue_ball_color.z, blue_ball_color.y, blue_ball_color.x), cv::FILLED, cv::LINE_AA);
		}
		
	}

	tuple<float, bool> assess_func(int balls) {
		float score = 0;
		uint64_t getting_ball = 0;
		for (const Log& log : logs)
		{
			if (log.type == BlockType::Ball)
			{
				++getting_ball;
			}
			else
			{
				score += 1.f * (1 + log.y / 7.f);
			}
		}

		score /= balls;

		bool died = false;

		for (int x = 0; x < Game_Width; ++x)
		{
			Block& block = Game_Map[x][Game_Height - 2];
			switch (block.type)
			{
			case BlockType::Block:
				died = true;
				break;
			case BlockType::Ball:
				++getting_ball;
				break;
			}
		}
		score += getting_ball * 5;

		return make_tuple(score, died);
	}

	void log_begin() {
		logs.clear();
	}

	GameAbstractData exportData() {
		GameAbstractData data;
		data.shoot_x = Game_X;
		for (int x = 0; x < Game_Width; ++x)
		{
			for (int y = 0; y < Game_Height; ++y)
			{
				const Block& block = Game_Map[x][y];
				switch (block.type)
				{
				case BlockType::Ball:
					data.num_map[x][y] = 0.f;
					data.type_map[x][y] = -1.f;
					break;
				case BlockType::Block:
					data.num_map[x][y] = block.num / (float)Game_Balls;
					data.type_map[x][y] = 1.f;
					break;
				case BlockType::None:
					data.num_map[x][y] = 0.f;
					data.type_map[x][y] = 0.f;
					break;
				}
			}
		}
		return data;
	}

	void importData(const GameAbstractData& data, const int& balls) {
		Game_X = data.shoot_x;
		Game_Balls = balls;

		for (int x = 0; x < Game_Width; ++x)
		{
			for (int y = 0; y < Game_Height; ++y)
			{
				if (data.type_map[x][y] == 1.f)
				{
					Game_Map[x][y] = Block(BlockType::Block, ceil(data.num_map[x][y] * balls));
				}
				else if (data.type_map[x][y] == -1.f)
				{
					Game_Map[x][y] = Block(BlockType::Ball, 1);
				}
				else
				{
					Game_Map[x][y] = Block(BlockType::None, 0);
				}
			}
		}
	}
};

void savedata(const GameAbstractData& data, int balls, int action, float score, int died)
{
	ofstream file("2.csv", fstream::in | fstream::out | fstream::app | fstream::binary);
	ostringstream ss;

	ss  << data.shoot_x << ","
		<< score << ","
		<< died << ","
		<< action << ",";

	for (int y = 1; y < Game_Height - 1; ++y)
	{
		for (int x = 0; x < Game_Width; ++x)
		{
			ss << data.type_map[x][y] << "," <<
				data.num_map[x][y] / balls << ",";
		}
	}
	ss << '\n';
	file << ss.str();
	file.close();
}

tuple<GameAbstractData, int, int> fabric_data()
{
	GameAbstractData data;

	//Clear
	for (int x = 0; x < Game_Width; ++x)
	{
		for (int y = 0; y < Game_Height; ++y)
		{
			data.num_map[x][y] = 0;
			data.type_map[x][y] = 0;
		}
	}

	//Set Environment
	int now_score = random() % 100 + 1;
	int now_ball = now_score;
	float proportion_block = random() % 100;

	//Set Header Ball
	int cursor = random() % Game_Width;
	data.type_map[cursor][1] = -1.f;

	//Set body
	for (int y = 2; y < Game_Height - 1; ++y)
	{
		if (now_ball > 1) {
			--now_ball;
			cursor = random() % Game_Width;
			data.type_map[cursor][y] = -1.f;
		}

		proportion_block = random() % 100;
		const int num = now_score - y + 1;
		if (now_score >= y)
		{
			for (int x = 0; x < Game_Width; ++x)
			{
				if (data.type_map[x][y] == 0.f && random() % 100 < proportion_block)
				{
					data.type_map[x][y] = 1.f;
					data.num_map[x][y] = (random() % (now_score - y + 1) + 1.f) / now_ball;
				}
			}
		}
	}

	//Set Header Block
	while (true)
	{
		cursor = random() % Game_Width;
		if (data.type_map[cursor][1] == 0.f)
		{
			data.type_map[cursor][1] = 1.f;
			data.num_map[cursor][1] = now_score / (float)now_ball;
			break;
		}
	}

	for (int x = 0; x < Game_Width; ++x)
	{
		if (data.type_map[x][1] == 0.f && random() % 100 < proportion_block)
		{
			data.type_map[x][1] = 1.f;
			data.num_map[x][1] = now_score / (float)now_ball;
		}
	}

	data.shoot_x = (random() % 10000) / 100.f;

	return make_tuple(data, now_ball, now_score);
}

float getBestDegreed(const GameAbstractData& data, int balls, int score)
{
	SBBGame Game;
	const static int precision = 128;
	int deg = 0;
	int best_deg = precision / 2;
	float best_score = 0;

	Game.Flag_under_clear = true;

	//float score_per_balls = score / (float)balls;
	//balls = 32;
	//score =  round(score_per_balls * balls);

	Game.custom_initialize(data, balls, score);
	while (Game.Flag_run)
	{
		switch (Game.Game_State)
		{
		case State::Shoot:
			Game.action_shoot(deg / (float)precision);
			break;
		case State::Result:
			{
				auto [score, died] = Game.assess_func(balls);
				for (int x = 0; x < Game_Width; ++x)
				{
					if (Game.Game_Map[x][Game_Height - 1].type == BlockType::Block ||
						Game.Game_Map[x][Game_Height - 2].type == BlockType::Block)
					{
						died = 1;
					}
				}
				if (died == 0)
				{
					//cout << deg << " " << score << endl;
					if (best_score < score)
					{
						best_score = score;
						best_deg = deg;
					}
				}
			}
			if (deg == precision)
			{
				//cout << best_score << endl;
				return best_deg / (float)precision;
			}
			else
			{
				++deg;
				Game.custom_initialize(data, balls, score);
				Game.log_begin();
			}
			break;
		}
		Game.loop();
	}

}

void getBestDegreedThread(const GameAbstractData& data, int balls, int score, std::atomic<float>& ret, std::atomic<bool>& load)
{
	const static int precision = 128;
	std::atomic<int> best_deg = precision / 2;
	std::atomic<float> best_score = 0;

	float score_per_balls = score / (float)balls;
	balls = 16;
	score = round(score_per_balls * balls);

	concurrency::parallel_for(0, precision, [&](int degreed) {
			SBBGame Game;
			Game.custom_initialize(data, balls, score);
			while (Game.Flag_run)
			{
				switch (Game.Game_State)
				{
				case State::Shoot:
					Game.action_shoot(degreed / (float)precision);
					break;
				case State::Result:
				{
					auto [score, died] = Game.assess_func(balls);
					if (died == 0)
					{
						if (best_score < score)
						{
							best_score = score;
							best_deg = degreed;
						}
					}
					Game.Flag_run = false;
				}
				break;
				}
				Game.loop();
			}
		}
	);

	load = true;
	ret = best_deg / (float)precision;
}

void runAIOnly()
{
	//Height, Width
	cv::Mat wind(1080, 1920, CV_8UC3);

	cv::namedWindow("mainWindow");
	cv::imshow("mainWindow", wind);
	cv::moveWindow("mainWindow", 0, 0);

	SBBGame gameAI;

	while (gameAI.Flag_run)
	{
		switch (gameAI.Game_State)
		{
		case State::Shoot:
			gameAI.action_shoot(getBestDegreed(gameAI.exportData(), gameAI.Game_Balls, gameAI.Game_Score));
			break;
		case State::Result:
			gameAI.Game_State = State::Prepare;
			gameAI.Game_X = gameAI.Game_NX;
			break;
		}
		gameAI.loop();
		if (gameAI.Game_t % 40 == 0)
		{
			gameAI.draw(wind);
			cv::imshow("mainWindow", wind);
			cv::waitKey(1);
		}

	}
}

void mouseCallback(int ev, int x, int y, int flags, void* data)
{
	(*((function<void(int, int, int, int)>*)data))(ev, x, y, flags);
}

void runPlayerOnly()
{
	//Height, Width
	cv::Mat wind(1080, 1920, CV_8UC3);

	cv::namedWindow("mainWindow");
	cv::setWindowProperty("mainWindow", cv::WINDOW_FULLSCREEN, cv::WINDOW_FULLSCREEN);
	
	cv::imshow("mainWindow", wind);
	cv::moveWindow("mainWindow", 0, 0);

	SBBGame gamePlayer;


	function<void(int, int, int, int)> playerMouse = [&](int ev, int x, int y, int flags) {
		switch (ev)
		{
		case cv::EVENT_LBUTTONDOWN:
			if (gamePlayer.Game_State == State::Shoot)
			{
				float rad = atan2f(
					gamePlayer.Game_X - (float)x / gamePlayer.display_Size * 100,
					gamePlayer.Game_Y - ((float)y - gamePlayer.display_DeltaY) / gamePlayer.display_Size * 100
				);
				
				rad = -DirectX::XM_PIDIV2 - rad;
				float deg = (rad - 0.15f + DirectX::XM_PI) / (DirectX::XM_PI - 0.3f);
				if (deg >= 0 && deg <= 1)
				{
					gamePlayer.action_shoot(deg);
				}
			}
			break;
		}
	};
	

	cv::setMouseCallback("mainWindow", mouseCallback, &playerMouse);

	while (gamePlayer.Flag_run)
	{
		switch (gamePlayer.Game_State)
		{
		case State::Result:
			gamePlayer.Game_State = State::Prepare;
			gamePlayer.Game_X = gamePlayer.Game_NX;
			break;
		}
		gamePlayer.loop();
		if (gamePlayer.Game_t % 40 == 0)
		{
			gamePlayer.draw(wind);
			cv::imshow("mainWindow", wind);
			cv::waitKey(1);
		}

	}
}

void loadRank(multimap<int, string, greater<int>>& ranks)
{
	ranks.clear();
	ifstream file("rank.data");

	if (file.fail() && !file.eof()) return;

	do
	{
		int first;
		string second;
		file >> second;
		if (file.eof()) break;
		file >> first;
		ranks.insert(pair(first, second));
	} while (!file.eof());
}

void saveRank(const multimap<int, string, greater<int>>& ranks)
{
	ofstream file("rank.data");

	for (auto itr : ranks)
	{
		file << itr.second << " " << itr.first << endl;
	}
}

void runBoth()
{
	//Height, Width
	cv::Mat wind(1080, 1920, CV_8UC3);

	cv::Rect displayPlayer = cv::Rect(160, 100, 640, 640);
	cv::Rect displayAI = cv::Rect(1120, 100, 640, 640);

	cv::Mat windPlayer(wind, displayPlayer);
	cv::Mat windAI(wind, displayAI);

	cv::namedWindow("mainWindow");
	cv::setWindowProperty("mainWindow", cv::WINDOW_FULLSCREEN, cv::WINDOW_FULLSCREEN);

	cv::imshow("mainWindow", wind);
	cv::moveWindow("mainWindow", 0, 0);

	std::atomic<float> bestDeg = 0.f;
	std::atomic<bool> load = false;

	thread trd;
	bool trd_run = false;
	GameState lancher_state = GameState::Login;
	char initial[3];
	int initial_cursor = 0;

	multimap<int, string, greater<int>> ranks;
	loadRank(ranks);

	SBBGame gamePlayer, gameAI, gameBackground;
	gamePlayer.back_color = { 200.f, 180.f, 180.f };
	gameAI.back_color = { 180.f, 180.f, 200.f };
	gameBackground.back_color = { 180.f, 180.f, 200.f };

	gamePlayer.fore_color = { 240.f, 240.f, 240.f };
	gameAI.fore_color = { 240.f, 240.f, 240.f };
	gameBackground.fore_color = { 240.f, 240.f, 240.f };

	const DirectX::XMFLOAT3 highest_color    = { 230.f,   5.f,   0.f };
	const DirectX::XMFLOAT3 lowest_color     = { 253.f, 204.f,  77.f };
	const DirectX::XMFLOAT3 green_ball_color = { 0.f, 255.f,  0.f };
	const DirectX::XMFLOAT3 blue_ball_color  = { 94.f, 167.f,  243.f };

	
	gameBackground.highest_color = blend(highest_color, gameBackground.fore_color, 0.25f);
	gameBackground.lowest_color  = blend(lowest_color, gameBackground.fore_color, 0.25f);
	gameBackground.green_ball_color = blend(green_ball_color, gameBackground.fore_color, 0.25f);
	gameBackground.blue_ball_color = blend(blue_ball_color, gameBackground.fore_color, 0.25f);

	gamePlayer.Flag_custom_state = true;
	gamePlayer.Flag_animation = true;
	gamePlayer.Flag_effect = true;
	gameAI.Flag_custom_state = true;
	gameAI.Flag_animation = true;
	gameAI.Flag_effect = true;
	
	gamePlayer.display_DeltaX = displayPlayer.x;
	gamePlayer.display_DeltaY = displayPlayer.y;
	gamePlayer.display_Size = displayPlayer.width;
	gameAI.display_DeltaX = displayAI.x;
	gameAI.display_DeltaY = displayAI.y;
	gameAI.display_Size = displayAI.width;
	gameBackground.display_DeltaX = displayAI.x;
	gameBackground.display_DeltaY = displayAI.y;
	gameBackground.display_Size = displayAI.width;
	
	vector<cv::Mat> icons;
	icons.push_back(cv::imread("icon/PLAYER_Q.png"));
	icons.push_back(cv::imread("icon/PLAYER_W.png"));
	icons.push_back(cv::imread("icon/PLAYER_E.png"));
	icons.push_back(cv::imread("icon/PLAYER_R.png"));
	icons.push_back(cv::imread("icon/AI_Q.png"));
	icons.push_back(cv::imread("icon/AI_W.png"));
	icons.push_back(cv::imread("icon/AI_E.png"));
	icons.push_back(cv::imread("icon/AI_R.png"));

	wind = cv::Scalar(255, 255, 255);

	for (int x = 0; x < 4 ; ++x)
	{
		icons[x].copyTo(wind(cv::Rect(89.6f + (89.6f + 128.f) * x, 770, 128, 128)));
		cv::rectangle(wind, cv::Rect(89.6f + (89.6f + 128.f) * x, 770, 128, 128), cv::Scalar(128, 128, 128), 5, cv::LINE_AA);
	}

	//
	for (int x = 0; x < 4; ++x)
	{
		icons[4 + x].copyTo(wind(cv::Rect(960 + 89.6f + (89.6f + 128.f) * x, 770, 128, 128)));
		cv::rectangle(wind, cv::Rect(960 + 89.6f + (89.6f + 128.f) * x, 770, 128, 128), cv::Scalar(128, 128, 128), 5, cv::LINE_AA);
	}

	float playerStack = 0;
	float aiStack = 0;
	float playerShowStack = 0;
	float aiShowStack = 0;
	const float playerSkillCost[4] = { 300.f, 400.f, 500.f, 700.f };
	const float aiSkillCost[4] = { 150.f, 360.f, 575.f, 700.f };
	bool playerSkillEnable[4] = { false ,false, false, false };
	bool playerSkillUse[4] = { false, false, false, false };
	bool aiSkillEnable[4] = { false, false, false, false };
	bool aiSkillUse[4] = { false, false, false, false };
	int aiWantToSkill = 0;
	int raise_ball = 0;

	function<void(int, int, int, int)> playerMouse = [&](int ev, int x, int y, int flags) {
		if (x >= gamePlayer.display_DeltaX && x <= gamePlayer.display_DeltaX + gamePlayer.display_Size &&
			y >= gamePlayer.display_DeltaY && y <= gamePlayer.display_DeltaY + gamePlayer.display_Size && lancher_state == GameState::Play)
		{
			switch (ev)
			{
			case cv::EVENT_MOUSEMOVE:
				if (gamePlayer.Game_State == State::Shoot) {

					float rad = atan2f(
						gamePlayer.Game_X - ((float)x - gamePlayer.display_DeltaX) / gamePlayer.display_Size * 100,
						gamePlayer.Game_Y - ((float)y - gamePlayer.display_DeltaY) / gamePlayer.display_Size * 100
					);

					rad = -DirectX::XM_PIDIV2 - rad;
					float deg = (rad - 0.15f + DirectX::XM_PI) / (DirectX::XM_PI - 0.3f);
					if (deg >= 0 && deg <= 1)
					{
						gamePlayer.guide_enable = true;
						gamePlayer.guide_line = deg;
					}
				}
				break;
			case cv::EVENT_LBUTTONDOWN:
				if (gamePlayer.Game_State == State::Shoot && gameAI.Game_State == State::Shoot && load && (!trd_run))
				{
					float rad = atan2f(
						gamePlayer.Game_X - ((float)x - gamePlayer.display_DeltaX) / gamePlayer.display_Size * 100,
						gamePlayer.Game_Y - ((float)y - gamePlayer.display_DeltaY) / gamePlayer.display_Size * 100
					);

					rad = -DirectX::XM_PIDIV2 - rad;
					float deg = (rad - 0.15f + DirectX::XM_PI) / (DirectX::XM_PI - 0.3f);
					//if (deg >= 0 && deg <= 1)
					if (deg < 0) deg = 0;
					if (deg > 1) deg = 1;
					{
						gamePlayer.action_shoot(deg);
						gameAI.action_shoot(bestDeg);
						gamePlayer.guide_enable = false;
						gameAI.guide_enable = false;
						load = false;
					}
				}
				break;
			}
		}
	};


	cv::setMouseCallback("mainWindow", mouseCallback, &playerMouse);

	while (gamePlayer.Flag_run)
	{
		switch (gamePlayer.Game_State)
		{
		case State::Animation:
			if (gamePlayer.Game_t % 40 == 0)
			{
				switch (gamePlayer.Game_Animation)
				{
				case AnimationState::GettingDowns:
				{
					static int remain;
					if (gamePlayer.Game_Animation_Progress == 0.f)
					{
						remain = (int)gamePlayer.display_Size / 9;
						gamePlayer.Game_Animation_Progress = 1.f;
					}
					gamePlayer.Game_Animation_Dy += remain / 2;
					remain /= 2;
					if (remain == 0)
					{
						gamePlayer.Game_Animation = AnimationState::None;
						gamePlayer.Game_State = State::Shoot;
						gamePlayer.Game_Animation_Dy = 0;

						for (int x = 0; x < Game_Width; ++x)
						{
							Block& block = gamePlayer.Game_Map[x][Game_Height - 1];
							switch (block.type)
							{
							case BlockType::Ball:
								++gamePlayer.Game_Balls;
								block = Block(BlockType::None, 0);
								if (gamePlayer.Flag_effect)
								{
									for (int _ = 0; _ < 15; ++_)
									{
										float rad = (_ / 7.5f) * DirectX::XM_2PI;
										DirectX::XMFLOAT3 color = { 0, 255, 0 };
										gamePlayer.Game_effects.push_back(
											Effect{
												100 / 6.f * (x + 0.5f),
												100 / 9.f * (Game_Height - 1 + 0.5f),
												2.f,
												rad,
												color
											});
									}
								}
								break;
							}
						}
					}
				
				}
				break;
				case AnimationState::Dying:
				{
					static float remain;
					if (gamePlayer.Game_Animation_Progress == 0.f)
					{
						remain = 0.f;
						gamePlayer.Game_Animation_Progress = 1.f;
					}
					remain += 0.04f;
					float mean_higest_color = (highest_color.x + highest_color.y + highest_color.z) / 3.f;
					float mean_lowest_color = (lowest_color.x + lowest_color.y + lowest_color.z) / 3.f;
					float mean_green_ball_color = (green_ball_color.x + green_ball_color.y + green_ball_color.z) / 3.f;
					float mean_blue_ball_color = (blue_ball_color.x + blue_ball_color.y + blue_ball_color.z) / 3.f;
					mean_higest_color = (gamePlayer.fore_color.x + mean_higest_color) / 2.f;
					mean_lowest_color = (gamePlayer.fore_color.x + mean_lowest_color) / 2.f;
					mean_green_ball_color = (gamePlayer.fore_color.x + mean_green_ball_color) / 2.f;
					mean_blue_ball_color = (gamePlayer.fore_color.x + mean_blue_ball_color) / 2.f;

					gamePlayer.lowest_color = blend({ mean_lowest_color , mean_lowest_color , mean_lowest_color }, lowest_color, remain);
					gamePlayer.highest_color = blend({ mean_higest_color , mean_higest_color , mean_higest_color }, highest_color, remain);
					gamePlayer.green_ball_color = blend({ mean_green_ball_color , mean_green_ball_color , mean_green_ball_color }, green_ball_color, remain);
					gamePlayer.blue_ball_color = blend({ mean_blue_ball_color , mean_blue_ball_color , mean_blue_ball_color }, blue_ball_color, remain);
					if (remain >= 1)
					{
						gamePlayer.Game_Animation = AnimationState::None;
						gamePlayer.Game_State = State::Die;
						lancher_state = GameState::GameOver;
						ranks.insert(pair<int, string>(gamePlayer.Game_Score, initial));
						saveRank(ranks);
					}
				}
				break;
				}
			}
			break;
		case State::Result:
			gamePlayer.Game_State = State::Prepare;
			gamePlayer.Game_X = gamePlayer.Game_NX;
			for (const Log& log : gamePlayer.logs)
			{
				if (log.type == BlockType::Block)
					playerStack += 10 / (1 + log.y / 5.f) / gamePlayer.Game_Balls;
			}
			gamePlayer.logs.clear();

			if (playerSkillEnable[0] && playerSkillUse[0])
			{
				gamePlayer.Game_Balls -= raise_ball;
				playerSkillEnable[0] = false;
				playerSkillUse[0] = false;
			}
			if (playerSkillEnable[1] && playerSkillUse[1])
			{
				playerSkillEnable[1] = false;
				playerSkillUse[1] = false;
			}
			if (playerSkillEnable[2] && playerSkillUse[2])
			{
				playerSkillEnable[2] = false;
				playerSkillUse[2] = false;
			}
			if (playerSkillEnable[3] && playerSkillUse[3])
			{
				playerSkillEnable[3] = false;
				playerSkillUse[3] = false;
			}

			if (aiSkillEnable[0] && aiSkillUse[0])
			{
				gamePlayer.Penalty_disable_BlockNumber = false;
				aiSkillEnable[0] = false;
				aiSkillUse[0] = false;
			}
			if (aiSkillEnable[1] && aiSkillUse[1])
			{
				gamePlayer.Penalty_disable_GuideLine = false;
				aiSkillEnable[1] = false;
				aiSkillUse[1] = false;
			}
			if (aiSkillEnable[2] && aiSkillUse[2])
			{
				aiSkillEnable[2] = false;
				aiSkillUse[2] = false;
			}
			if (aiSkillEnable[3] && aiSkillUse[3])
			{
				gamePlayer.Flag_roofOff = false;
				aiSkillEnable[3] = false;
				aiSkillUse[3] = false;
			}
			break;
		case State::Prepare:
			if (gameAI.Game_State == State::Prepare)
			{
				const auto data = create_BoardHeader(gameAI.Game_Score);
				gamePlayer.action_prepare(data);
				gameAI.action_prepare(data);

				if (aiSkillEnable[2])
				{
					for (int x = 0; x < 4; ++x)
					{
						if (playerSkillEnable[x] && !playerSkillUse[x])
						{
							playerSkillEnable[x] = false;
						}
					}
				}

				if (playerSkillEnable[1])
				{
					for (int x = 0; x < Game_Width; ++x)
					{
						for (int y = 0; y < Game_Height; ++y)
						{
							if (gameAI.Game_Map[x][y].type == BlockType::Block)
							{
								gameAI.Game_Map[x][y].num *= 2;
							}
						}
					}
					playerSkillUse[1] = true;
				}
				if (playerSkillEnable[2])
				{
					for (int y = 0; y < Game_Height; y += 2)
					{
						for (int x = 0; x < Game_Width; ++x)
						{
							if (gamePlayer.Game_Map[x][y].type != BlockType::Ball && gameAI.Game_Map[x][y].type != BlockType::Ball)
							{
								Block temp = gameAI.Game_Map[x][y];
								gameAI.Game_Map[x][y] = gamePlayer.Game_Map[x][y];
								gamePlayer.Game_Map[x][y] = temp;
							}
						}
					}
					playerSkillUse[2] = true;
				}
				if (playerSkillEnable[3])
				{
					int num = 0;
					for (int y = Game_Height - 3; y > 0; --y)
					{
						for (int x = 0; x < Game_Width; ++x)
						{
							if (gameAI.Game_Map[x][y].type == BlockType::None && (x + y) % 2 == 0)
							{
								gameAI.Game_Map[x][y] = Block(BlockType::Block, 1 + num * (gameAI.Game_Balls) / 4);
							}
						}
						++num;
					}
					playerSkillUse[3] = true;
				}

				if (aiSkillEnable[0])
				{
					gamePlayer.Penalty_disable_BlockNumber = true;
					aiSkillUse[0] = true;
				}
				if (aiSkillEnable[1])
				{
					gamePlayer.Penalty_disable_GuideLine = true;
					aiSkillUse[1] = true;
				}
				if (aiSkillEnable[2])
				{
					aiSkillUse[2] = true;
				}
				if (aiSkillEnable[3])
				{
					gamePlayer.Flag_roofOff = true;
					aiSkillUse[3] = true;
				}
			}
			break;
		case State::Shooting:
			if (gamePlayer.logs.size())
			{
				for (const Log& log : gamePlayer.logs)
				{
					if (log.type == BlockType::Block)
						playerStack += 10 / (1 + log.y / 5.f) / gamePlayer.Game_Balls;

				}
				gamePlayer.logs.clear();
			}
			break;
		}
		switch (gameAI.Game_State)
		{
		case State::Animation:
			if (gameAI.Game_t % 40 == 0)
			{
				switch (gameAI.Game_Animation)
				{
				case AnimationState::GettingDowns:
				{
					static int remain;
					if (gameAI.Game_Animation_Progress == 0.f)
					{
						remain = (int)gameAI.display_Size / 9;
						gameAI.Game_Animation_Progress = 1.f;
					}
					gameAI.Game_Animation_Dy += remain / 2;
					remain /= 2;
					if (remain == 0)
					{
						gameAI.Game_Animation = AnimationState::None;
						gameAI.Game_State = State::Shoot;
						gameAI.Game_Animation_Dy = 0;

						for (int x = 0; x < Game_Width; ++x)
						{
							Block& block = gameAI.Game_Map[x][Game_Height - 1];
							switch (block.type)
							{
							case BlockType::Ball:
								++gameAI.Game_Balls;
								block = Block(BlockType::None, 0);
								if (gameAI.Flag_effect)
								{
									for (int _ = 0; _ < 15; ++_)
									{
										float rad = (_ / 7.5f) * DirectX::XM_2PI;
										DirectX::XMFLOAT3 color = { 0, 255, 0 };
										gameAI.Game_effects.push_back(
											Effect{
												100 / 6.f * (x + 0.5f),
												100 / 9.f * (Game_Height - 1 + 0.5f),
												2.f,
												rad,
												color
											});
									}
								}
								break;
							}
						}
					}

				}
				break;
				case AnimationState::Dying:
				{
					static float remain;
					if (gameAI.Game_Animation_Progress == 0.f)
					{
						remain = 0.f;
						gameAI.Game_Animation_Progress = 1.f;
					}
					remain += 0.04f;
					float mean_higest_color = (highest_color.x + highest_color.y + highest_color.z) / 3;
					float mean_lowest_color = (lowest_color.x + lowest_color.y + lowest_color.z) / 3;
					float mean_green_ball_color = (green_ball_color.x + green_ball_color.y + green_ball_color.z) / 3;
					float mean_blue_ball_color = (blue_ball_color.x + blue_ball_color.y + blue_ball_color.z) / 3;
					mean_higest_color = (gameAI.fore_color.x + mean_higest_color) / 2.f;
					mean_lowest_color = (gameAI.fore_color.x + mean_lowest_color) / 2.f;
					mean_green_ball_color = (gameAI.fore_color.x + mean_green_ball_color) / 2.f;
					mean_blue_ball_color = (gameAI.fore_color.x + mean_blue_ball_color) / 2.f;
					gameAI.lowest_color = blend({ mean_lowest_color , mean_lowest_color , mean_lowest_color }, lowest_color, remain);
					gameAI.highest_color = blend({ mean_higest_color , mean_higest_color , mean_higest_color }, highest_color, remain);
					gameAI.green_ball_color = blend({ mean_green_ball_color , mean_green_ball_color , mean_green_ball_color }, green_ball_color, remain);
					gameAI.blue_ball_color = blend({ mean_blue_ball_color , mean_blue_ball_color , mean_blue_ball_color }, blue_ball_color, remain);
					if (remain >= 1)
					{
						gameAI.Game_Animation = AnimationState::None;
						gameAI.Game_State = State::Die;
						lancher_state = GameState::GameOver;
						ranks.insert(pair<int, string>(gamePlayer.Game_Score, initial));
						saveRank(ranks);
					}
				}
				break;
				}
			}
			break;
		case State::Shoot:
			if (!load)
			{
				if (!trd_run)
				{
					gameAI.fore_color = { 200.f, 200.f, 200.f };
					gamePlayer.blue_ball_color = { 100.f, 100.f, 100.f };
					trd_run = true;
					trd = thread(getBestDegreedThread, gameAI.exportData(), gameAI.Game_Balls, gameAI.Game_Score, ref(bestDeg), ref(load));
				}
			}
			else
			{
				if (trd_run)
				{
					gameAI.fore_color = { 240.f, 240.f, 240.f };
					gamePlayer.blue_ball_color = blue_ball_color;
					gameAI.guide_enable = true;
					gameAI.guide_line = bestDeg;
					trd_run = false;
					trd.join();
				}
			}
			break;
		case State::Result:
			gameAI.Game_State = State::Prepare;
			gameAI.Game_X = gameAI.Game_NX;
			for (const Log& log : gameAI.logs)
			{
				if (log.type == BlockType::Block)
					aiStack += 10 / (1 + log.y / 5.f) / gameAI.Game_Balls;
			}
			gameAI.logs.clear();
			break;
		case State::Shooting:
			if (gameAI.logs.size())
			{
				for (const Log& log : gameAI.logs)
				{
					if (log.type == BlockType::Block)
						aiStack += 10 / (1 + log.y / 5.f) / gameAI.Game_Balls;
				}
				gameAI.logs.clear();
				
				bool able = false;
				for (int x = 0; x < 4; ++x)
				{
					if (playerSkillEnable[x] && !playerSkillUse[x])
					{
						able = true;
						break;
					}
				}

				if (able)
					aiWantToSkill = 2;
				else if (aiWantToSkill == 2)
				{
					able = false;
					for (int x = 0; x < 4; ++x)
					{
						if (playerSkillEnable[x] && !playerSkillUse[x])
						{
							able = true;
							break;
						}
					}
					if (!able)
					{
						do {
							aiWantToSkill = random() % 4;
						} while (aiWantToSkill == 2);
					}
				}
			
				if (aiSkillCost[aiWantToSkill] <= aiStack && !aiSkillEnable[aiWantToSkill])
				{
					aiStack -= aiSkillCost[aiWantToSkill];
					aiSkillEnable[aiWantToSkill] = true;
					aiWantToSkill = random() % 4;
				}
			}
			break;
		}
		gamePlayer.loop();
		gameAI.loop();

		if (lancher_state == GameState::Login)
		{
			switch (gameBackground.Game_State)
			{
			case State::Shoot:
				gameBackground.action_shoot((random() % 129) / 128.f);
				break;
			case State::Result:
				gameBackground.Game_State = State::Prepare;
				break;
			case State::Die:
				gameBackground.initialize();
				break;
			}
			gameBackground.loop();
		}

		int time = gamePlayer.Game_Score + 25;
		if (gamePlayer.Game_t % time == 0)
		{
			gamePlayer.draw(windPlayer);
			if (lancher_state == GameState::Login)
			{
				gameBackground.draw(windAI);
				const static array<string, 7> order_num = {
					"1ST", "2ND", "3RD", "4th", "5th", "6th", "7th"
				};

				auto itr = ranks.cbegin();
				for (int y = 0; y < 7; ++y)
				{
					{
						char buf[8] = "___    ";
						
						if (itr != ranks.cend())
							sprintf_s(buf, "%3s %3d", itr->second, itr->first);
						

						const cv::String str = order_num[y] + " " + buf;
						const cv::Size& size = cv::getTextSize(str, cv::HersheyFonts::FONT_HERSHEY_DUPLEX, y < 3 ? 2.f : 1.5f, 4.f, nullptr);
						cv::putText(wind, str, cv::Point(
							displayAI.x + displayAI.width / 2 - size.width / 2,
							displayAI.y + (y + 1.5f) * displayAI.height / 8 - size.height / 4),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, y < 3 ? 2.f : 1.5f, cv::Scalar(40, 40, 40), 4.f);
					}
					if (ranks.cend() != itr) ++itr;
				}
			}
			else
			{
				gameAI.draw(windAI);
			}

			{
				cv::Mat(wind, cv::Rect(0, 0, 1920, 100)) = cv::Scalar(255, 255, 255);
				char buf[256];
				int total_score = 0;
				for (int x = 0; x < Game_Width; ++x)
				{
					for (int y = 0; y < Game_Height; ++y)
					{
						if (gamePlayer.Game_Map[x][y].type == BlockType::Block)
						{
							total_score += gamePlayer.Game_Map[x][y].num;
						}
					}
				}
				sprintf_s(buf, "Human:%5d", total_score);
				const cv::String str = buf;
				const cv::Size& size = cv::getTextSize(str, cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, 2.f, nullptr);
				cv::putText(wind, str, cv::Point(displayPlayer.x + displayPlayer.width / 2 - size.width / 2, 50 + size.height / 2),
					cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, cv::Scalar(0, 0, 0), 2.f, cv::LINE_AA);
			}

			{
				char buf[256];
				int total_score = 0;
				for (int x = 0; x < Game_Width; ++x)
				{
					for (int y = 0; y < Game_Height; ++y)
					{
						if (gameAI.Game_Map[x][y].type == BlockType::Block)
						{
							total_score += gameAI.Game_Map[x][y].num;
						}
					}
				}

				sprintf_s(buf, " A.I :%5d", total_score);
				const cv::String str = buf;
				const cv::Size& size = cv::getTextSize(str, cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, 2.f, nullptr);
				cv::putText(wind, str, cv::Point(displayAI.x + displayAI.width / 2 - size.width / 2, 50 + size.height / 2),
					cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, cv::Scalar(0, 0, 0), 2.f, cv::LINE_AA);
			}
			{
				const cv::String qwer[4] = { "Q", "W", "E", "R" };
				if (playerStack > 785) playerStack = 785;
				if (aiStack > 785) aiStack = 785;

				playerShowStack += (playerStack - playerShowStack) * 0.0004f * time;
				aiShowStack += (aiStack - aiShowStack) * 0.0004f * time;

				cv::rectangle(wind, cv::Rect(87, 920, 787, 40), cv::Scalar(80, 80, 80), cv::FILLED, cv::LINE_AA);
				if (playerShowStack > playerStack)
				{
					cv::rectangle(wind, cv::Rect(88, 921, playerShowStack, 38), cv::Scalar(160, 200, 160), cv::FILLED, cv::LINE_AA);
					cv::rectangle(wind, cv::Rect(88, 921, playerStack, 38), cv::Scalar(80, 200, 80), cv::FILLED, cv::LINE_AA);
				}
				else
				{
					cv::rectangle(wind, cv::Rect(88, 921, playerStack, 38), cv::Scalar(160, 200, 160), cv::FILLED, cv::LINE_AA);
					cv::rectangle(wind, cv::Rect(88, 921, playerShowStack, 38), cv::Scalar(80, 200, 80), cv::FILLED, cv::LINE_AA);
				}
				for (int x = 0; x < 4; ++x)
				{
					cv::line(wind, cv::Point(87 + playerSkillCost[x], 920), cv::Point(87 + playerSkillCost[x], 960), cv::Scalar(200, 200, 200), 2, cv::LINE_AA);
					const cv::Size& size = cv::getTextSize(qwer[x], cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, 2.f, nullptr);
					cv::putText(wind, qwer[x], cv::Point(90 + playerSkillCost[x], 939 + size.height / 2),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, cv::Scalar(200, 200, 200), 2.f, cv::LINE_AA);

					DirectX::XMFLOAT3 color;
					bool animate = true;
					if (playerSkillEnable[x])
						color = { 244.f, 238.f, 0.f };
					else
					{
						if (aiSkillUse[2])
							color = { 255.f, 0.f, 0.f };
						else
						{
							if (playerStack >= playerSkillCost[x])
								color = { 100.f, 200.f, 100.f };
							else
							{
								color = { 128.f, 128.f, 128.f };
								animate = false;
							}
						}
					}

					if (animate)
					{
						float value = ((gamePlayer.Game_t / time) % 31) / 30.f;
						value = powf(0.5f - value, 2) * 1 + 0.75f;
						color.x *= value;
						color.y *= value;
						color.z *= value;
					}

					cv::rectangle(wind, cv::Rect(89.6f + (89.6f + 128.f) * x, 770, 128, 128), cv::Scalar(color.z, color.y, color.x), 5, cv::LINE_AA);
				}

				cv::rectangle(wind, cv::Rect(1047, 920, 787, 40), cv::Scalar(80, 80, 80), cv::FILLED, cv::LINE_AA);
				if (aiShowStack > aiStack)
				{
					cv::rectangle(wind, cv::Rect(1048, 921, aiShowStack, 38), cv::Scalar(160, 200, 160), cv::FILLED, cv::LINE_AA);
					cv::rectangle(wind, cv::Rect(1048, 921, aiStack, 38), cv::Scalar(80, 200, 80), cv::FILLED, cv::LINE_AA);
				}
				else
				{
					cv::rectangle(wind, cv::Rect(1048, 921, aiStack, 38), cv::Scalar(160, 200, 160), cv::FILLED, cv::LINE_AA);
					cv::rectangle(wind, cv::Rect(1048, 921, aiShowStack, 38), cv::Scalar(80, 200, 80), cv::FILLED, cv::LINE_AA);
				}
				for (int x = 0; x < 4; ++x)
				{
					cv::line(wind, cv::Point(1047 + aiSkillCost[x], 920), cv::Point(1047 + aiSkillCost[x], 960), cv::Scalar(200, 200, 200), 2, cv::LINE_AA);
					const cv::Size& size = cv::getTextSize(qwer[x], cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, 2.f, nullptr);
					cv::putText(wind, qwer[x], cv::Point(1050 + aiSkillCost[x], 939 + size.height / 2),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, cv::Scalar(200, 200, 200), 2.f, cv::LINE_AA);

					DirectX::XMFLOAT3 color;
					bool animate = true;
					if (aiSkillEnable[x])
						color = { 244.f, 238.f, 0.f };
					else
					{
						{
							if (aiStack >= aiSkillCost[x])
								color = { 100.f, 200.f, 100.f };
							else
							{
								color = { 128.f, 128.f, 128.f };
								animate = false;
							}
						}
					}

					if (animate)
					{
						float value = ((gamePlayer.Game_t / time) % 31) / 30.f;
						value = powf(0.5f - value, 2) * 1 + 0.75f;
						color.x *= value;
						color.y *= value;
						color.z *= value;
					}

					cv::rectangle(wind, cv::Rect(960 + 89.6f + (89.6f + 128.f) * x, 770, 128, 128), cv::Scalar(color.z, color.y, color.x), 5, cv::LINE_AA);
				}

			}

			if (lancher_state == GameState::Login)
			{
				{
					char buf[256];
					sprintf_s(buf, "Type Initial : %c%c%c",
						initial_cursor > 0 ? initial[0] : '_',
						initial_cursor > 1 ? initial[1] : '_',
						initial_cursor > 2 ? initial[2] : '_');
					const cv::String str = buf;
					const cv::Size& size = cv::getTextSize(str, cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 2.f, 4.f, nullptr);
					cv::putText(wind, str, cv::Point(displayPlayer.x + displayPlayer.width / 2 - size.width / 2, displayPlayer.y + displayPlayer.height / 2 + size.height / 2),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 2.f, cv::Scalar(40, 40, 40), 4.f);
				}

				if (initial_cursor == 3 && (gameAI.Game_t / 1600) % 2 == 0)
				{
					const cv::String str = "PRESS ENTER TO START";
					const cv::Size& size = cv::getTextSize(str, cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, 2.f, nullptr);
					cv::putText(wind, str, cv::Point(displayPlayer.x + displayPlayer.width / 2 - size.width / 2, displayPlayer.y + displayPlayer.height * 5 / 8 + size.height / 2),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, cv::Scalar(40, 40, 40), 2.f);
				}
			}
			if (lancher_state == GameState::GameOver)
			{
				{
					const cv::String str = "Game Over";
					const cv::Size& size = cv::getTextSize(str, cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 2.f, 4.f, nullptr);
					cv::putText(wind, str, cv::Point(displayPlayer.x + displayPlayer.width / 2 - size.width / 2 + 3, displayPlayer.y + displayPlayer.height / 2 - size.height / 4 + 3),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 2.f, cv::Scalar(40, 40, 40), 6.f);
					cv::putText(wind, str, cv::Point(displayPlayer.x + displayPlayer.width / 2 - size.width / 2, displayPlayer.y + displayPlayer.height / 2 - size.height / 4),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 2.f, cv::Scalar(255, 255, 255), 4.f);
				}
				{
					const cv::String str = "Total Score : " +  to_string(gamePlayer.Game_Score);
					const cv::Size& size = cv::getTextSize(str, cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 2.f, 4.f, nullptr);
					cv::putText(wind, str, cv::Point(displayPlayer.x + displayPlayer.width / 2 - size.width / 2 + 3, displayPlayer.y + displayPlayer.height / 2 + size.height * 5 / 4 + 3),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 2.f, cv::Scalar(40, 40, 40), 6.f, cv::LINE_AA);
					cv::putText(wind, str, cv::Point(displayPlayer.x + displayPlayer.width / 2 - size.width / 2, displayPlayer.y + displayPlayer.height / 2 + size.height * 5 / 4),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 2.f, cv::Scalar(255, 255, 255), 4.f, cv::LINE_AA);
				}
				{
					const cv::String str = "PRESS ENTER TO RESTART";
					const cv::Size& size = cv::getTextSize(str, cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, 2.f, nullptr);
					cv::putText(wind, str, cv::Point(displayPlayer.x + displayPlayer.width / 2 - size.width / 2 + 3, displayPlayer.y + displayPlayer.height *  5 / 8 + size.height / 2 + 3),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, cv::Scalar(40, 40, 40), 2.f);
					cv::putText(wind, str, cv::Point(displayPlayer.x + displayPlayer.width / 2 - size.width / 2, displayPlayer.y + displayPlayer.height * 5 / 8 + size.height / 2),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, cv::Scalar(255, 255, 255), 2.f);
				}
			}


			cv::imshow("mainWindow", wind);
			char input = cv::waitKey(1);
			
			if (lancher_state == GameState::Play)
			{
				if (!trd_run)
				{
					switch (input)
					{
					case 'q':
						if ((gamePlayer.Game_State != State::Prepare && gamePlayer.Game_State != State::Result && gamePlayer.Game_State != State::Shooting) && !(playerSkillEnable[0]) && playerSkillCost[0] <= playerStack && !aiSkillUse[2])
						{
							playerSkillEnable[0] = true;
							playerSkillUse[0] = true;
							raise_ball = gamePlayer.Game_Balls * 2;
							gamePlayer.Game_Balls += raise_ball;
							playerStack -= playerSkillCost[0];
						}
						break;
					case 'w':
						if ((gamePlayer.Game_State != State::Prepare && gamePlayer.Game_State != State::Result) && !(playerSkillEnable[1]) && playerSkillCost[1] <= playerStack && !aiSkillUse[2])
						{
							playerSkillEnable[1] = true;
							playerSkillUse[1] = false;
							playerStack -= playerSkillCost[1];
						}
						break;
					case 'e':
						if ((gamePlayer.Game_State != State::Prepare && gamePlayer.Game_State != State::Result) && !(playerSkillEnable[2]) && playerSkillCost[2] <= playerStack && !aiSkillUse[2])
						{
							playerSkillEnable[2] = true;
							playerSkillUse[2] = false;
							playerStack -= playerSkillCost[2];
						}
						break;
					case 'r':
						if ((gamePlayer.Game_State != State::Prepare && gamePlayer.Game_State != State::Result) && !(playerSkillEnable[3]) && playerSkillCost[3] <= playerStack && !aiSkillUse[2])
						{
							playerSkillEnable[3] = true;
							playerSkillUse[3] = false;
							playerStack -= playerSkillCost[3];
						}
						break;
					case 27:
						if (gamePlayer.Game_State == State::Shoot)
						{
							gamePlayer.Game_State = State::Animation;
							gamePlayer.Game_Animation = AnimationState::Dying;
							gamePlayer.Game_Animation_Progress = 0.f;
						}
						break;
					}
				}
			}
			else if (lancher_state == GameState::Login)
			{
				if (input >= 'a' && input <= 'z' && initial_cursor < 3)
				{
					initial[initial_cursor++] = input - 'a' + 'A';
				}
				if (input >= 'A' && input <= 'Z' && initial_cursor < 3)
				{
					initial[initial_cursor++] = input;
				}
				if (input == 8 && initial_cursor > 0)
				{
					--initial_cursor;
				}
				if (input == '0')
				{
					gamePlayer.Flag_run = false;
				}
				if (input == 13 && initial_cursor == 3  && !trd_run)
				{
					lancher_state = GameState::Play;
					initial_cursor = 0;
				}
			}
			else if (lancher_state == GameState::GameOver)
			{
				if (input == 13)
				{
					lancher_state = GameState::Login;
					gamePlayer.initialize();
					gameAI.initialize();

					gamePlayer.fore_color = { 240.f, 240.f, 240.f };
					gameAI.fore_color = { 240.f, 240.f, 240.f };

					gamePlayer.lowest_color = lowest_color;
					gamePlayer.highest_color = highest_color;
					gamePlayer.green_ball_color = green_ball_color;
					gamePlayer.blue_ball_color = blue_ball_color;

					gameAI.lowest_color = lowest_color;
					gameAI.highest_color = highest_color;
					gameAI.green_ball_color = green_ball_color;
					gameAI.blue_ball_color = blue_ball_color;


					if (playerSkillEnable[0] && playerSkillUse[0])
					{
						gamePlayer.Game_Balls /= 3;
						playerSkillEnable[0] = false;
						playerSkillUse[0] = false;
					}
					if (playerSkillEnable[1] && playerSkillUse[1])
					{
						playerSkillEnable[1] = false;
						playerSkillUse[1] = false;
					}
					if (playerSkillEnable[2] && playerSkillUse[2])
					{
						playerSkillEnable[2] = false;
						playerSkillUse[2] = false;
					}
					if (playerSkillEnable[3] && playerSkillUse[3])
					{
						playerSkillEnable[3] = false;
						playerSkillUse[3] = false;
					}

					if (aiSkillEnable[0] && aiSkillUse[0])
					{
						gamePlayer.Penalty_disable_BlockNumber = false;
						aiSkillEnable[0] = false;
						aiSkillUse[0] = false;
					}
					if (aiSkillEnable[1] && aiSkillUse[1])
					{
						gamePlayer.Penalty_disable_GuideLine = false;
						aiSkillEnable[1] = false;
						aiSkillUse[1] = false;
					}
					if (aiSkillEnable[2] && aiSkillUse[2])
					{
						aiSkillEnable[2] = false;
						aiSkillUse[2] = false;
					}
					if (aiSkillEnable[3] && aiSkillUse[3])
					{
						gamePlayer.Flag_roofOff = false;
						aiSkillEnable[3] = false;
						aiSkillUse[3] = false;
					}

					aiStack = 0;
					playerStack = 0;
					aiShowStack = 0;
					playerShowStack = 0;
				}
			}
		}

	}
}


int main()
{
	//SetPriorityClass(GetCurrentProcess(), 0x100);
	runBoth();

	return 0;
}