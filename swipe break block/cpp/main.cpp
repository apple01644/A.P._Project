#include <iostream>
#include <fstream>
#include <sstream>

#include <vector>
#include <time.h>
#include <random>
#define _USE_MATH_DEFINES
#include <math.h>

#include <opencv2/opencv.hpp>

#define _AMD64_
#include <processthreadsapi.h>

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

const static int Game_Width = 6;
const static int Game_Height = 9;

struct GameAbstractData {
	float shoot_x;
	float type_map[Game_Width][Game_Height];
	float num_map[Game_Width][Game_Height];

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

	const float dt = 0.1f;
	const float ball_rad = 100.f / Game_Height * 0.185f;
	const float Game_Y = 100 - ball_rad;

	std::vector<Log> logs;

	cv::HersheyFonts font_face = cv::HersheyFonts::FONT_HERSHEY_DUPLEX;
	double font_size = 0.0025 * 480;
	int font_thick = 3;

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

		int left_ball = Game_Score / 5 + 3;
		for (int x = 0; x < Game_Width; ++x)
		{
			Block& block = Game_Map[x][0];
			if (block.type == BlockType::None)
			{
				if (random() % 100 < 50)
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
								float rad = atan2f(vec.y, vec.x) / (float)M_PI;
								if (rad >= 1.f / 4 && rad < 3.f / 4)
									ball.vy = abs(ball.vy);
								else if (rad >= 3.f / 4 || rad < -3.f / 4)
									ball.vx = -abs(ball.vx);
								else if (rad >= -3.f / 4 && rad < -1.f / 4)
									ball.vy = -abs(ball.vy);
								else if (rad >= -1.f / 4 || rad < 1.f / 4)
									ball.vx = abs(ball.vx);
							}

							if (block.lastHitId != ball.id || block.lastHitTime + dt * 15 < Game_t)
							{
								--block.num;
								logs.push_back(Log{ x, y, block.num, BlockType::Block });
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
								logs.push_back(Log{x, y, 0, BlockType::Ball});
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
		mat = cv::Scalar(255, 255, 255);
		for (int x = 0; x < Game_Width; ++x)
		{
			for (int y = 0; y < Game_Height; ++y)
			{
				switch (Game_Map[x][y].type)
				{
				case BlockType::Block:
					{ 
						cv::Mat(mat, cv::Rect(480 * x / 6, 480 * y / 9, 480 / 6, 480 / 9)) = cv::Scalar(0, 127.f - 127.f * Game_Map[x][y].num / Game_Score, 255.f);
						const cv::String str = to_string(Game_Map[x][y].num);
						const cv::Size& size = cv::getTextSize(str, font_face, font_size, font_thick, nullptr);
						cv::putText(mat, str, cv::Point(480 * (x + 0.5f) / 6 - size.width / 2, 480 * (y + 0.5f) / 9 + size.height / 2), font_face, font_size, cv::Scalar(255, 255, 255), font_thick);
					}
					break;
				case BlockType::Ball:
					cv::circle(mat, cv::Point(480 * (x + 0.5f) / 6, 480 * (y + 0.5f) / 9), ball_rad * 480.f / 100, cv::Scalar(0, 255, 0), cv::FILLED);
					break;
				}
			}
		}

		for (const Ball& ball : Game_shootBalls)
		{
			cv::circle(mat, cv::Point(480 * ball.x / 100, 480 * ball.y / 100), ball_rad * 480.f / 100, cv::Scalar(255, 255, 0), cv::FILLED);
		}

		const cv::String str = "x" + to_string((Game_State == State::Shooting) ? Game_LeftBalls : Game_Balls);
		const cv::Size& size = cv::getTextSize(str, font_face, font_size / 2, font_thick / 2, nullptr);
		cv::putText(mat, str, cv::Point(Game_X * 480 / 100 - size.width / 2, 480 * 8.5f / 9 + size.height / 2), font_face, font_size / 2, cv::Scalar(255, 255, 0), font_thick / 2);
		cv::circle(mat, cv::Point(Game_X * 480 / 100 , 480 - ball_rad * 240.f / 100), ball_rad * 480.f / 100, cv::Scalar(255, 255, 0), cv::FILLED);

		cv::imshow("mainWindow", mat);
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

int main()
{
	cv::Mat wind(489, 489, CV_8UC3);
	SetPriorityClass(GetCurrentProcess(), 0x100);

	SBBGame game;
	GameAbstractData data;
	const int action_count = 32;

	int deg = 0;
	int balls;

	int best_deg = 0;
	float best_score = 0;
	float second_score = 0;

	{
		const auto false_data = fabric_data();
		data = get<0>(false_data);
		balls = get<1>(false_data);
		game.custom_initialize(data, balls, get<2>(false_data));
	}
	
	while (game.Flag_run)
	{
		switch (game.Game_State)
		{
		case State::Shoot:
			if (deg == 0)
			{
				balls = game.Game_Balls;
				best_deg = 0;
				best_score = 0;
				second_score = 0;
			}
			cout << "\r" << deg << " / " << action_count << " is completed";
			game.action_shoot((deg / (float)action_count));
			++deg;
			break;
		case State::Result:
			float score;
			bool died;

			tie(score, died) = game.assess_func(game.Game_Balls);

			if ((died ? 0 : score) > best_score)
			{
				second_score = best_score;
				best_score = score;
				best_deg = deg - 1;
			}
			else if ((died ? 0 : score) > second_score)
			{
				second_score = score;
			}
			savedata(data, balls, deg - 1, score, 0);

			if (deg == action_count + 1)
			{
				cout << "\r" << "state fully simulated. Best degreed is " << best_deg << " and it get " << best_score << "pts " << endl;
				
				const auto false_data = fabric_data();
				data = get<0>(false_data);
				balls = get<1>(false_data);
				game.custom_initialize(data, balls, get<2>(false_data));

				deg = 0;
			}
			else
			{
				game.importData(data, balls);
				game.Game_t = 0;
				game.Game_State = State::Shoot;
				game.log_begin();
			}
			break;
		}
		game.loop();
		/*if (game.Game_t % 10 == 0)
		{
			game.draw(wind);
			cv::waitKey(1);
		}*/

	}

	return 0;
}