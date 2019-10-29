#include <iostream>
#include <fstream>
#include <sstream>

#include <vector>
#include <time.h>
#include <random>

#include <thread>
#include <future>
#include <functional>

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
	bool Flag_effect = true;
	bool Flag_run = true;
	bool Flag_animation = false;
	bool Flag_roofOff = false;

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
					block.lastHitId = -1;
					block.lastHitTime = 0;
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
							if ((ball.y > (y * 100 / 9.f + 0.1f) + (100 / 9.f - 0.2f) || ball.y < (y * 100 / 9.f + 0.1f)) &&
								(ball.x > (x * 100 / 6.f + 0.1f) + (100 / 6.f - 0.2f) || ball.x < (x * 100 / 6.f + 0.1f)))
							{
								Point<float> nearest;
								Point<int> point;

								if (ball.x > (x * 100 / 6.f + 1) + (100 / 6.f - 2) / 2)
								{
									nearest.x = (x * 100 / 6.f + 1) + (100 / 6.f - 2);
									point.x = 1;
								}
								else
								{
									nearest.x = (x * 100 / 6.f + 1);
									point.x = 0;
								}

								if (ball.y > (y * 100 / 9.f + 1) + (100 / 9.f - 2) / 2)
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

								passed[0][0] = ((0 <= x - 1) && (x - 1 < 6) && (Game_Map[x - 1][y].type != BlockType::Block));
								passed[0][1] = ((0 <= x + 1) && (x + 1 < 6) && (Game_Map[x + 1][y].type != BlockType::Block));

								passed[1][0] = ((0 <= y - 1) && (y - 1 < 9) && (Game_Map[x][y - 1].type != BlockType::Block));
								passed[1][1] = ((0 <= y + 1) && (y + 1 < 9) && (Game_Map[x][y + 1].type != BlockType::Block));

								if (passed[0][point.x] && passed[1][point.y])
								{
									Point<float>vec{ nearest.x - ball.x, nearest.y - ball.y };
									float rad_C2P = atan2f(vec.y, vec.x) + DirectX::XM_PIDIV2;
									float rad = atan2f(ball.vx, ball.vy);
									float length = sqrtf(powf(ball.vy, 2) + powf(ball.vx, 2));
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

							if ((block.lastHitId != ball.id || block.lastHitTime + 1.5f / dt < Game_t) && block.num > 0)
							{
								--block.num;
								logs.push_back(Log{ x, y, block.num, BlockType::Block });
								block.lastHitId = ball.id;
								block.lastHitTime = Game_t;
								if (block.num == 0)
								{
									if (Flag_effect)
									{
										DirectX::XMFLOAT3 color = blend(highest_color, lowest_color, Game_Map[x][y].num / (float)Game_Score);
										for (int _ = 0; _ < 30; ++_)
										{
											float rad = (_ / 15.f) * DirectX::XM_2PI;
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
									for (int _ = 0; _ < 30; ++_)
									{
										float rad = (_ / 15.f) * DirectX::XM_2PI;
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
					break;
				case BlockType::Ball:
					if (block.num <= 0)
					{
						block.type = BlockType::None;
						++Game_Balls;
					}
					break;
				}
			}

		vector<Ball> balls;
		balls.swap(Game_shootBalls);

		for (Ball& ball : balls)
		{
			if (ball.y <= Game_Y && (ball.y >= ball_rad * 1.5f || !Flag_roofOff))
			{
				Game_shootBalls.push_back(ball);
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
				}
			}
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

			int time = Penalty_disable_GuideLine ? 32660 : 0;
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
								if ((pt.y > (y * 100 / 9.f + 0.1f) + (100 / 9.f - 0.2f) || pt.y < (y * 100 / 9.f + 0.1f)) &&
									(pt.x > (x * 100 / 6.f + 0.1f) + (100 / 6.f - 0.2f) || pt.x < (x * 100 / 6.f + 0.1f)))
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

									passed[0][0] = ((0 <= x - 1) && (x - 1 < 6) && (Game_Map[x - 1][y].type != BlockType::Block));
									passed[0][1] = ((0 <= x + 1) && (x + 1 < 6) && (Game_Map[x + 1][y].type != BlockType::Block));

									passed[1][0] = ((0 <= y - 1) && (y - 1 < 9) && (Game_Map[x][y - 1].type != BlockType::Block));
									passed[1][1] = ((0 <= y + 1) && (y + 1 < 9) && (Game_Map[x][y + 1].type != BlockType::Block));

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

					if (++x % 2 == 0)cv::line(mat, (*itr_f) * display_Size / 100.f, (*itr_b) * display_Size / 100.f, cv::Scalar(243, 167, 94), 3, cv::LINE_AA);
					++itr_f;
					++itr_b;
				}

				cv::circle(mat, cv::Point(display_Size* itr_f->x / 100, display_Size * itr_f->y / 100), ball_rad* display_Size / 100, cv::Scalar(243, 167, 94), cv::FILLED, cv::LINE_AA);
			}

		}

		if (Flag_roofOff)
		{
			float y = 0;
			for (int x = 0; x < 16; ++x)
			{
				cv::line(mat, cv::Point(0, y), cv::Point(display_Size, y) , cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
				y += sqrtf(x) / 2;
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
					cv::circle(mat, cv::Point(display_Size * (x + 0.5f) / 6, display_Size * (y + 0.5f) / 9) + animationDelta, (ball_rad + 0.625f + 0.3f * (sinf(Game_t / 60.f) / 2 + 0.5f)) * display_Size / 100, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
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
		score += getting_ball * 5;

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
				died = true;
				score += 5.f;
				break;
			}
		}

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

	float score_per_balls = score / (float)balls;
	balls = 32;
	score =  round(score_per_balls * balls);

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

	SBBGame gamePlayer, gameAI;
	gamePlayer.back_color = { 200.f, 180.f, 180.f };
	gameAI.back_color = { 180.f, 180.f, 200.f };

	gamePlayer.fore_color = { 240.f, 240.f, 240.f };
	gameAI.fore_color = { 240.f, 240.f, 240.f };

	const DirectX::XMFLOAT3 highest_color = { 230.f,   5.f,   0.f };
	const DirectX::XMFLOAT3  lowest_color = { 253.f, 204.f,  77.f };
	const DirectX::XMFLOAT3  green_ball_color = { 0.f, 255.f,  0.f };
	const DirectX::XMFLOAT3  blue_ball_color = { 94.f, 167.f,  243.f };

	gamePlayer.Flag_custom_state = true;
	gameAI.Flag_custom_state = true;
	gamePlayer.Flag_animation = true;

	gamePlayer.display_DeltaX = displayPlayer.x;
	gamePlayer.display_DeltaY = displayPlayer.y;
	gamePlayer.display_Size = displayPlayer.width;
	gameAI.display_DeltaX = displayAI.x;
	gameAI.display_DeltaY = displayAI.y;
	gameAI.display_Size = displayAI.width;
	
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
	const float playerSkillCost[4] = { 200.f, 300.f, 400.f, 700.f };
	const float aiSkillCost[4] = { 250.f, 300.f, 375.f, 500.f };
	bool playerSkillEnable[4] = { false ,false, false, false };
	bool playerSkillUse[4] = { false, false, false, false };
	bool aiSkillEnable[4] = { false, false, false, false };
	bool aiSkillUse[4] = { false, false, false, false };
	int aiWantToSkill = random() % 4;

	function<void(int, int, int, int)> playerMouse = [&](int ev, int x, int y, int flags) {
		if (x >= gamePlayer.display_DeltaX && x <= gamePlayer.display_DeltaX + gamePlayer.display_Size &&
			y >= gamePlayer.display_DeltaY && y <= gamePlayer.display_DeltaY + gamePlayer.display_Size)
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
					if (deg >= 0 && deg <= 1)
					{
						gamePlayer.action_shoot(deg);
						gameAI.action_shoot(bestDeg);
						gamePlayer.guide_enable = false;
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
									for (int _ = 0; _ < 30; ++_)
									{
										float rad = (_ / 15.f) * DirectX::XM_2PI;
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
					float mean_higest_color = (highest_color.x + highest_color.y + highest_color.z) / 3;
					float mean_lowest_color = (lowest_color.x + lowest_color.y + lowest_color.z) / 3;
					float mean_green_ball_color = (green_ball_color.x + green_ball_color.y + green_ball_color.z) / 3;
					float mean_blue_ball_color = (blue_ball_color.x + blue_ball_color.y + blue_ball_color.z) / 3;
					gamePlayer.lowest_color = blend({ mean_lowest_color , mean_lowest_color , mean_lowest_color }, lowest_color, remain);
					gamePlayer.highest_color = blend({ mean_higest_color , mean_higest_color , mean_higest_color }, highest_color, remain);
					gamePlayer.green_ball_color = blend({ mean_green_ball_color , mean_green_ball_color , mean_green_ball_color }, green_ball_color, remain);
					gamePlayer.blue_ball_color = blend({ mean_blue_ball_color , mean_blue_ball_color , mean_blue_ball_color }, blue_ball_color, remain);
					if (remain >= 1)
					{
						gamePlayer.Game_Animation = AnimationState::None;
						gamePlayer.Game_State = State::Die;
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
					playerStack += 1 / 3.f / log.y;
			}
			gamePlayer.logs.clear();

			if (playerSkillEnable[0] && playerSkillUse[0])
			{
				gamePlayer.Game_Balls /= 2;
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
				aiSkillEnable[1] = false;
				aiSkillUse[1] = false;
			}
			if (aiSkillEnable[2] && aiSkillUse[2])
			{
				gamePlayer.Penalty_disable_GuideLine = false;
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

				if (playerSkillEnable[0])
				{
					gamePlayer.Game_Balls *= 2;
					playerSkillUse[0] = true;
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
							Block temp = gameAI.Game_Map[x][y];
							gameAI.Game_Map[x][y] = gamePlayer.Game_Map[x][y];
							gamePlayer.Game_Map[x][y] = temp;
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
								gameAI.Game_Map[x][y] = Block(BlockType::Block, 1 + num * (gameAI.Game_Balls) / 2);
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
					aiSkillUse[1] = true;
				}
				if (aiSkillEnable[2])
				{
					gamePlayer.Penalty_disable_GuideLine = true;
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
						playerStack += 1 / 3.f / log.y;
				}
				gamePlayer.logs.clear();
			}
			break;
		}
		switch (gameAI.Game_State)
		{
		case State::Shoot:
			if (!load)
			{
				if (!trd_run)
				{
					gameAI.fore_color = { 200.f, 200.f, 200.f };
					trd_run = true;
					trd = thread(getBestDegreedThread, gameAI.exportData(), gameAI.Game_Balls, gameAI.Game_Score, ref(bestDeg), ref(load));
				}
			}
			else
			{
				if (trd_run)
				{
					gameAI.fore_color = { 240.f, 240.f, 240.f };
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
					aiStack += 1 / 3.f / log.y;
			}
			gameAI.logs.clear();
			break;
		case State::Shooting:
			if (gameAI.logs.size())
			{
				for (const Log& log : gameAI.logs)
				{
					if (log.type == BlockType::Block)
						aiStack += 1 / 3.f / log.y;
				}
				gameAI.logs.clear();
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
		if (gamePlayer.Game_t % 40 == 0)
		{
			gamePlayer.draw(windPlayer);
			gameAI.draw(windAI);

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

				cv::rectangle(wind, cv::Rect(87, 920, 787, 40), cv::Scalar(80, 80, 80), cv::FILLED, cv::LINE_AA);
				cv::rectangle(wind, cv::Rect(88, 921, playerStack, 38), cv::Scalar(80, 200, 80), cv::FILLED, cv::LINE_AA);
				for (int x = 0; x < 4; ++x)
				{
					cv::line(wind, cv::Point(87 + playerSkillCost[x], 920), cv::Point(87 + playerSkillCost[x], 960), cv::Scalar(200, 200, 200), 2, cv::LINE_AA);
					const cv::Size& size = cv::getTextSize(qwer[x], cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, 2.f, nullptr);
					cv::putText(wind, qwer[x], cv::Point(90 + playerSkillCost[x], 939 + size.height / 2),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, cv::Scalar(200, 200, 200), 2.f, cv::LINE_AA);

					cv::rectangle(wind, cv::Rect(89.6f + (89.6f + 128.f) * x, 770, 128, 128), playerSkillEnable[x] ? cv::Scalar(0, 238, 244) : (aiSkillUse[1] ? cv::Scalar(0, 0, 255) :(playerStack >= playerSkillCost[x] ? cv::Scalar(100, 200, 100) : cv::Scalar(128, 128, 128))), 5, cv::LINE_AA);
				}

				cv::rectangle(wind, cv::Rect(1047, 920, 787, 40), cv::Scalar(80, 80, 80), cv::FILLED, cv::LINE_AA);
				cv::rectangle(wind, cv::Rect(1047, 921, aiStack, 38), cv::Scalar(80, 200, 80), cv::FILLED, cv::LINE_AA);
				for (int x = 0; x < 4; ++x)
				{
					cv::line(wind, cv::Point(1047 + aiSkillCost[x], 920), cv::Point(1047 + aiSkillCost[x], 960), cv::Scalar(200, 200, 200), 2, cv::LINE_AA);
					const cv::Size& size = cv::getTextSize(qwer[x], cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, 2.f, nullptr);
					cv::putText(wind, qwer[x], cv::Point(1050 + aiSkillCost[x], 939 + size.height / 2),
						cv::HersheyFonts::FONT_HERSHEY_DUPLEX, 1.f, cv::Scalar(200, 200, 200), 2.f, cv::LINE_AA);

					cv::rectangle(wind, cv::Rect(960 + 89.6f + (89.6f + 128.f) * x, 770, 128, 128), aiSkillEnable[x] ? cv::Scalar(0, 238, 244) : (aiStack >= aiSkillCost[x] ? cv::Scalar(100, 200, 100) : cv::Scalar(128, 128, 128)), 5, cv::LINE_AA);
				}

			}


			cv::imshow("mainWindow", wind);
			char input = cv::waitKey(1);
			
			if (!trd_run)
			{
				switch (input)
				{
				case 27:
					gamePlayer.Flag_run = false;
					break;
				case '1'://1
					if (!trd_run)
					{
						for (int x = 0; x < 4; ++x)
						{
							playerSkillEnable[x] = false;
							playerSkillUse[x] = false;
							playerStack = 0;

							aiSkillEnable[x] = false;
							aiSkillUse[x] = false;
							aiStack = 0;
						}

						gamePlayer.Flag_roofOff = false;
						gamePlayer.Penalty_disable_BlockNumber = false;
						gamePlayer.Penalty_disable_GuideLine = false;

						gamePlayer.initialize();
						gamePlayer.Game_shootBalls.clear();

						gameAI.initialize();
						gameAI.Game_shootBalls.clear();
					}
					break;
				case 'q':
					if (!trd_run && (gamePlayer.Game_State != State::Prepare && gamePlayer.Game_State != State::Result) && !(playerSkillEnable[0]) && playerSkillCost[0] <= playerStack && !aiSkillUse[1])
					{
						playerSkillEnable[0] = true;
						playerSkillUse[0] = false;
						playerStack -= playerSkillCost[0];
					}
					break;
				case 'w':
					if (!trd_run && (gamePlayer.Game_State != State::Prepare && gamePlayer.Game_State != State::Result) && !(playerSkillEnable[1]) && playerSkillCost[1] <= playerStack && !aiSkillUse[1])
					{
						playerSkillEnable[1] = true;
						playerSkillUse[1] = false;
						playerStack -= playerSkillCost[1];
					}
					break;
				case 'e':
					if (!trd_run && (gamePlayer.Game_State != State::Prepare && gamePlayer.Game_State != State::Result) && !(playerSkillEnable[2]) && playerSkillCost[2] <= playerStack && !aiSkillUse[1])
					{
						playerSkillEnable[2] = true;
						playerSkillUse[2] = false;
						playerStack -= playerSkillCost[2];
					}
					break;
				case 'r':
					if (!trd_run && (gamePlayer.Game_State != State::Prepare && gamePlayer.Game_State != State::Result) && !(playerSkillEnable[3]) && playerSkillCost[3] <= playerStack && !aiSkillUse[1])
					{
						playerSkillEnable[3] = true;
						playerSkillUse[3] = false;
						playerStack -= playerSkillCost[3];
					}
					break;
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