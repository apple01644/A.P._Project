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

class SBBGame {
public:
	Block Game_Map[Game_Width][Game_Height];

	std::vector<Ball> Game_shootBalls;
	int Game_Score;
	float Game_X, Game_NX;
	int Game_LeftBalls, Game_Balls;
	State Game_State;
	uint64_t Game_LastShoot = 0;
	uint64_t Game_t = 0, attempt = 0;
	float Game_ShootRad;
	bool Game_groundBall = false;

	bool Flag_user = false;
	bool Flag_custom_state = false;
	bool Flag_run = true;
	bool Flag_effect = true;

	const float dt = 0.1f;
	const float ball_rad = 100.f / Game_Height * 0.185f;
	const float Game_Y = 100 - ball_rad;

	std::list<Log> logs;

	cv::HersheyFonts font_face = cv::HersheyFonts::FONT_HERSHEY_DUPLEX;
	double font_size = 0.0025 * 480;
	int font_thick = 3;

	const float display_DeltaY = (1048 - 960) / 2;
	const float display_Size = 960;

	const DirectX::XMFLOAT3 highest_color = { 230.f,   5.f,   0.f };
	const DirectX::XMFLOAT3  lowest_color = { 253.f, 204.f,  77.f };
	const DirectX::XMFLOAT3  hitted_color = { 255.f, 255.f, 255.f };

	cv::String window_name;
	int window_x, window_y;

	std::list<Effect> Game_effects;
	

	SBBGame() {
		initialize();
	}

	void action_prepare()
	{
		int cursor = random() % Game_Width;
		Game_Map[cursor][0] = Block(BlockType::Ball, 1);

		while (true)
		{
			cursor = random() % Game_Width;
			if (Game_Map[cursor][0].type == BlockType::None)
			{
				Game_Map[cursor][0] = Block(BlockType::Block, Game_Score);
				break;
			}
		}

		int left_ball = 5;
		for (int x = 0; x < Game_Width; ++x)
		{
			Block& block = Game_Map[x][0];
			if (block.type == BlockType::None)
			{
				if (random() % 100 < 50 + Game_Score)
				{
					Game_Map[x][0] = Block(BlockType::Block, Game_Score);
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
				initialize();
				return;
			case BlockType::Ball:
				++Game_Balls;
				block = Block(BlockType::None, 0);
				break;
			}
		}

		++Game_Score;
		Game_State = State::Shoot;
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
							if ((ball.y > (y * 100 / 9.f + 1) + (100 / 9.f - 2) || ball.y < (y * 100 / 9.f + 1)) &&
								(ball.x > (x * 100 / 6.f + 1) + (100 / 6.f - 2) || ball.x < (x * 100 / 6.f + 1)))
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
			if (ball.y <= Game_Y)
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

	void draw(const cv::Mat& _mat) {
		cv::Mat mat = _mat.clone();
		mat = cv::Scalar(200, 200, 200);
		cv::Mat(mat, cv::Rect(0, display_DeltaY, display_Size, display_Size)) = cv::Scalar(255, 255, 255);

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
					cv::rectangle(mat, cv::Rect((effect.x - effect.size / 2) / 100 * display_Size, (effect.y - effect.size / 2) / 100 * display_Size + display_DeltaY, effect.size / 100 * display_Size, effect.size / 100 * display_Size), cv::Scalar(effect.color.z, effect.color.y, effect.color.x), cv::FILLED, cv::LINE_AA);

					effect.x += cosf(effect.rad);
					effect.y += sinf(effect.rad);

					effect.rad += 0.06f;
					effect.size -= 0.09f;
					effect.color.x += 0.03f;
					effect.color.y += 0.03f;
					effect.color.z += 0.03f;
					
					Game_effects.push_back(effect);
				}
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
						if (Game_t - Game_Map[x][y].lastHitTime < 60)
						{
							color = blend(color, hitted_color, (Game_t - Game_Map[x][y].lastHitTime) / 120.f + 0.5f);							
						}
						cv::Mat(mat, cv::Rect(display_Size * x / 6.f, display_Size * y / 9.f + display_DeltaY, display_Size / 6.f - 1, display_Size / 9.f - 1)) = cv::Scalar(color.z, color.y, color.x);
						const cv::String str = to_string(Game_Map[x][y].num);
						const cv::Size& size = cv::getTextSize(str, font_face, font_size, font_thick, nullptr);
						cv::putText(mat, str, cv::Point(display_Size * (x + 0.5f) / 6 - size.width / 2, display_Size * (y + 0.5f) / 9 + size.height / 2 + display_DeltaY), font_face, font_size, cv::Scalar(255, 255, 255), font_thick);
					}
					break;
				case BlockType::Ball:
					cv::circle(mat, cv::Point(display_Size * (x + 0.5f) / 6, display_Size * (y + 0.5f) / 9 + display_DeltaY), (ball_rad + 0.625f + 0.3f * (sinf(Game_t / 60.f) / 2 + 0.5f)) * display_Size / 100, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
					cv::circle(mat, cv::Point(display_Size * (x + 0.5f) / 6, display_Size * (y + 0.5f) / 9 + display_DeltaY), ball_rad * display_Size / 100, cv::Scalar(0, 255, 0), cv::FILLED, cv::LINE_AA);
					break;
				}
			}
		}

		for (const Ball& ball : Game_shootBalls)
		{
			cv::circle(mat, cv::Point(display_Size * ball.x / 100, display_Size * ball.y / 100 + display_DeltaY), ball_rad * display_Size / 100, cv::Scalar(243, 167, 94), cv::FILLED, cv::LINE_AA);
		}
		
		if (Game_State != State::Shooting || Game_LeftBalls > 0)
		{
			const cv::String str = "x" + to_string((Game_State == State::Shooting) ? Game_LeftBalls : Game_Balls);
			const cv::Size& size = cv::getTextSize(str, font_face, font_size * 0.67f, font_thick * 0.67f, nullptr);
			cv::putText(mat, str, cv::Point(Game_X * display_Size / 100 - size.width / 2, display_Size * 8.5f / 9 + size.height / 2 + display_DeltaY), font_face, font_size * 0.67f, cv::Scalar(243, 167, 94), font_thick * 0.67f);
			cv::circle(mat, cv::Point(Game_X * display_Size / 100, display_Size - ball_rad * display_Size / 100 + display_DeltaY), ball_rad * display_Size / 100, cv::Scalar(243, 167, 94), cv::FILLED, cv::LINE_AA);
		}

		cv::imshow(window_name, mat);
		cv::moveWindow(window_name, window_x, window_y);
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

void runAIOnly()
{
	//Height, Width
	cv::Mat wind1(1048, 960, CV_8UC3);
	SetPriorityClass(GetCurrentProcess(), 0x100);

	SBBGame gameAI;
	gameAI.window_name = "AI";
	gameAI.window_x = (1920 - 960) / 2;
	gameAI.window_y = 0;

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
			gameAI.draw(wind1);
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
	cv::Mat wind2(1048, 960, CV_8UC3);
	SetPriorityClass(GetCurrentProcess(), 0x100);

	SBBGame gamePlayer;
	gamePlayer.window_name = "Player";
	gamePlayer.window_x = (1920 - 960) / 2;
	gamePlayer.window_y = 0;

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
				
				//if (rad > DirectX::XM_PIDIV2) rad -= DirectX::XM_2PI;
				//gamePlayer.Game_shootBalls.push_back(Ball{ (float)x / gamePlayer.display_Size * 100, ((float)y - gamePlayer.display_DeltaY) / gamePlayer.display_Size * 100, 0, 0, 0 });
				//gamePlayer.Game_shootBalls.push_back(Ball{ gamePlayer.Game_X, gamePlayer.Game_Y, 0, 0, 0 });
				rad = DirectX::XM_PIDIV2 - rad;
				cout << rad << endl;
				float deg = (rad - 0.15f + DirectX::XM_PI) / (DirectX::XM_PI - 0.3f);
				if (deg >= 0 && deg <= 1)
				{
					gamePlayer.action_shoot(deg);
				}
			}
			break;
		}
	};
	
	cv::namedWindow(gamePlayer.window_name);
	cv::setMouseCallback(gamePlayer.window_name, mouseCallback, &playerMouse);

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
			gamePlayer.draw(wind2);
			cv::waitKey(1);
		}

	}
}

int main()
{
	runPlayerOnly();

	return 0;
}