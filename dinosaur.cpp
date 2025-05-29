#include <graphics.h>
#include <conio.h>
#include <vector>
#include <iostream>
#include <string>
#include <time.h>

#define QWORD unsigned long long
#define block(sx, sy, ex, ey, cor) 	setfillcolor(cor);\
									solidrectangle(sx, sy, ex, ey - 1);

using namespace std;

struct icon
{
	DWORD ofstx, ofsty;
	DWORD *iconbuf;
};

extern DWORD replay[], dinosaur_running1[], dinosaur_running2[], dinosaur_bending1[], dinosaur_bending2[], dinosaur_jumping[], dinosaur_die[];
extern DWORD cactus1[], cactus2[], cactus3[], cactus4[], cactus5[], cactus6[];// width = 30pixel 
extern DWORD cactus_large1[], cactus_large2[], cactus_large3[], cactus_large4[], cactus_large5[];// width = 44pixel(large5 : 100 pixel)
extern DWORD cloud[];
extern DWORD crow1[], crow2[];
extern QWORD road[];

DWORD *cactuses[11] = {
	cactus1, cactus2, cactus3, cactus4, cactus5, cactus6,
	cactus_large1, cactus_large2, cactus_large3, cactus_large4, cactus_large5
};// ������Ϊһ�����飬����֮�������ϰ�
DWORD cactussx[11] = { 30, 30, 30, 30, 30, 30, 44, 44, 44, 44, 100 };// ÿ�������ƵĿ��
DWORD crowy[3] = {
	960 / 4 * 3 - 100 /*��֤������ֱ����ʱ��Ҳ����ͨ��*/, 960 / 4 * 3 - 72 + 34 /*������Ҫ��������Ծ�ſ�ͨ��*/, 960 / 4 * 3 + 86 - 72/*����ֻ����Ծͨ��*/
};//���������鷽���������ѻ�ĸ߶�
DWORD *daynight_needchange[22] = {
	cactus1, cactus2, cactus3, cactus4, cactus5, cactus6,
	cactus_large1, cactus_large2, cactus_large3, cactus_large4, cactus_large5,
	crow1, crow2,
	replay, dinosaur_running1, dinosaur_running2, dinosaur_bending1, dinosaur_bending2, dinosaur_jumping, dinosaur_die
};// �ڰ��졢��ҹת������Ҫ�ı���ɫ��

bool ingame;
int cloudnum = 0, lastcacts = 0, bkcor = 0xff, time_obstmove;
double roadx[2] = { 0, 2404 };

//=========������ͨ�ú���=========
// ��������Ϊ EasyX �е� put|getpixel ����̫���ˣ��Լ�����д��һ���汾��
inline COLORREF getpixel(int x, int y, DWORD *buffer)
{
	static int sx = getwidth(), sy = getheight();
	return (x < 0 || y < 0 || x >= sx || y >= sy) ? 0 : BGR(buffer[y * sx + x]);
}

// ��ӡһ��ͼ��
void printicon(DWORD *icon, double dx, double dy)
{
	int x = (int)dx, y = (int)dy;
	for (DWORD i = 5; i < icon[0]; i++)// ���ν����������Ϣ
	{
		DWORD sx = icon[i] & 0x7f, sy = (icon[i] >> 7) & 0x7f, ex = (icon[i] >> 14) & 0xff, ey = (icon[i] >> 22) & 0xff;
		block(sx + x, sy + y, ex + x, ey + y, icon[1 + ((icon[i] >> 30) & 3)]);
	}
}

// ��ӡһ��ͼ�꣬QWORD ��ʽ 
void printicon(QWORD *icon, double dx, double dy)
{
	int x = (int)dx, y = (int)dy;
	for (int i = 2; i < icon[0]; i++)// ���ν����������Ϣ
	{
		DWORD sx = icon[i] & 0xfff, sy = (icon[i] >> 12) & 0xfff, ex = (icon[i] >> 24) & 0xfff, ey = (icon[i] >> 36) & 0xfff;
		block(sx + x, sy + y, ex + x, ey + y, (DWORD)icon[1]);
	}
}

// ���һ��ͼ���Ƿ�������ָ����ɫ
bool collide(DWORD *icon, double dxofst, double dyofst, COLORREF cor)// ���������ɫcor
{
	int xofst = (int)dxofst, yofst = (int)dyofst;
	DWORD *buf = GetImageBuffer();
	for (DWORD i = 5; i < icon[0]; i++)// ���ν����������Ϣ
	{
		DWORD sx = icon[i] & 0x7f, sy = (icon[i] >> 7) & 0x7f, ex = (icon[i] >> 14) & 0xff, ey = (icon[i] >> 22) & 0xff;
		for (DWORD x = sx + xofst; x < ex + xofst; x++)
			for (DWORD y = sy + yofst; y < ey + yofst; y++)
				if (getpixel(x, y, buf) == cor)
					return true;
	}
	return false;
}

// ������ת��Ϊ�ַ���(���������ڸ��������Դ˴������Ǵ��������
string int2string(long long a)
{
	if (a == 0)
		return "0";
	string ans = "";
	for (; a; a /= 10)
		ans += (a % 10) + '0';
	reverse(ans.begin(), ans.end());
	return ans;
}

// �������������һ�� struct icon
icon make_icon(DWORD x, DWORD y, DWORD *buf)
{
	icon ret;
	ret.ofstx = x;
	ret.ofsty = y;
	ret.iconbuf = buf;
	return ret;
}

//============��������Ϸģ��===========
class obstacle
{
private:
	bool need_collision;		// ��Ҫ��ײ���
	bool need_switch;			// ��Ҫ�л�ͼ��
	DWORD iconp, time_switch;	// ����л�ͼ�� 
	vector<icon> icons;			// ������ͼ��
	double movespeed_times;		// �����ƶ��ٶȵı���
	DWORD time_lastdraw;		// �ϴλ��Ƶ�ʱ�� 
public:
	double x, y;				// �ϰ��ﵱǰ���ڵ�����

	obstacle(double sx, double sy, bool needcoli)
	{
		x = sx;
		y = sy;
		movespeed_times = 1;
		need_collision = needcoli;
		need_switch = false;
		iconp = 0;
	}
	
	// ���ϰ���������ͼ��
	void add(icon ico)
	{
		icons.push_back(ico);
	}
	
	// �ƶ�ָ������
	void move(double delta)
	{
		x -= delta * movespeed_times;
	}
	
	// ����Ļ�ϻ��� 
	void draw()
	{
		if (need_switch)
		{
			printicon(icons[iconp].iconbuf, x + icons[iconp].ofstx, y + icons[iconp].ofsty);
			if (GetTickCount() - time_lastdraw > time_switch)
			{
				iconp = (iconp + 1) % icons.size();
				time_lastdraw += time_switch;
			}
			return;
		}

		for (DWORD i = 0; i < icons.size(); i++)
			printicon(icons[i].iconbuf, x + icons[i].ofstx, y + icons[i].ofsty);
	}

	// �Ƿ���������ɫ
	bool collide_with(COLORREF cor)
	{
		if (!need_collision)
			return false;
		for (DWORD i = 0; i < icons.size(); i++)
			if (collide(icons[i].iconbuf, x + icons[i].ofstx, y + icons[i].ofsty, cor))
				return true;
		return false;
	}
	
	// �����ٶȱ��� 
	void setspeedtimes(double tm)
	{
		movespeed_times = tm;
	}

	// ���ö���ʱ���л�һ��ͼ��
	void set_switch(int tmswt)
	{
		need_switch = true;
		time_switch = tmswt;
		time_lastdraw = GetTickCount();
	}

	icon &get_icon(int p)
	{
		return icons[p];
	}
};

// ������Ϸ��ʼǰ��׼��
void init()
{
	setbkcolor(WHITE);
	settextcolor(0x535353);
	setbkmode(TRANSPARENT);
	settextstyle(30, 15, "Ink Free");
	srand((unsigned)time(0));

	cleardevice();
	printicon(dinosaur_jumping, 1600 / 5, 960 / 4 * 3);
	printicon(replay, 1600 / 2 - 20, 960 / 2 - 20);
	printicon(road, 0, 960 / 4 * 3 + 80);
	outtextxy(1600 / 2 - 20 - 155, 960 / 2 + 50, "Press Enter or Spase to Start");

	flushmessage(EX_KEY); 
	for (ExMessage msg = getmessage(EX_KEY); msg.vkcode != VK_RETURN && msg.vkcode != VK_SPACE; msg = getmessage(EX_KEY));
	BeginBatchDraw();
}

// ���ƿ���
void draw_dinosaur_once(int ofstx, int ofsty, bool nojump/*û������*/, bool bend)
{
	static bool twinkle = false;
	static QWORD time = GetTickCount();

	if (GetTickCount() - time >= 150)// ÿ150ms�任һ�α��ܶ���
	{
		twinkle = !twinkle;
		time = GetTickCount();
	}

	ofsty += bend ? 34 : 0;

	if (nojump)
		if (!bend)
			printicon((!twinkle) ? dinosaur_running1 : dinosaur_running2, 0 + ofstx, 0 + ofsty);
		else
			printicon((!twinkle) ? dinosaur_bending1 : dinosaur_bending2, 0 + ofstx, 0 + ofsty);
	else
		printicon(dinosaur_jumping, 0 + ofstx, 0 + ofsty);

}

// ����ģ��
void dinosaur_jump_fall(int &dinosaur_ofstx, int &dinosaur_ofsty, int &fall_v, int &time_fall, bool &running, bool bend)
{
	if ((fall_v < 0 || dinosaur_ofsty < 960 / 4 * 3))
	{
		for (; GetTickCount() - time_fall > 15; time_fall += 15, fall_v++)// ÿ 15ms ����λ��
			dinosaur_ofsty = min(dinosaur_ofsty + fall_v, 960 / 4 * 3);
	}
	else
	{
		fall_v = 0;
		running = true;
	}
}

// ����ϰ���
void add_obstacle(vector<obstacle> &obst, double speed, double score)
{
	static int cldmktime = 0, lstcldy = 0;

	if (obst.size() - cloudnum == 0 || obst[lastcacts].x < 1600 / 5 || ((rand() % 80 == 1) && \
		(obst[lastcacts].x < 1600 - 15 * 18 * 2 * min(speed, 0.5))))
	{
		if (score > 200 && rand() % 11 == 1)// ������ѻ
		{
			obstacle tmp(1600, crowy[rand() % 3], true);
			tmp.add(make_icon(0, 12, crow1));
			tmp.add(make_icon(0, 0, crow2));
			if (rand() % 2 == 1)
				swap(tmp.get_icon(0), tmp.get_icon(1));
			tmp.set_switch(250);
			tmp.setspeedtimes(1.01);
			obst.push_back(tmp);
			lastcacts = obst.size() - 1;
		}
		else
		{
			obstacle tmp(1600, 960 / 4 * 3, true);
			DWORD chs = rand() % 11;
			tmp.add(make_icon(0, (DWORD)((chs < 6) ? 26 : 0), cactuses[chs]));
			if (rand() % 3 < 2 && chs != 10 && chs != 5)// ��������������ϰ�
				tmp.add(make_icon(cactussx[chs] + 1, (DWORD)((chs < 6) ? 26 : 0), cactuses[chs]));
			obst.push_back(tmp);
			lastcacts = obst.size() - 1;
		}
	}

	if (!cloudnum || rand() % 500 == 1 && GetTickCount() - cldmktime > 557)
	{
		cloudnum++;

		int cldy = rand() % 200;
		cldy += (cldy < lstcldy + 30 && cldy > lstcldy - 30) ? rand() % 2 ? -30 : 30 : 0;

		obstacle tmp(1600, cldy, false);
		tmp.add(make_icon(0, 0, cloud));
		tmp.setspeedtimes(0.25);
		obst.push_back(tmp);

		cldmktime = GetTickCount();
		lstcldy = cldy;
	}
}

// ��ײ���&&�ƶ�&&��������
void move_and_collide(vector<obstacle> &obst, double speed, double &score)
{
	if (obst.size())
	{
		DWORD nowtime = GetTickCount();
		for (DWORD i = 0; i < obst.size(); i++)
		{
			obst[i].move((nowtime - time_obstmove) * speed);
			if (obst[i].collide_with(daynight_needchange[0][1]))
				ingame = false;
			obst[i].draw();
		}

		roadx[1] -= speed * (nowtime - time_obstmove);
		roadx[0] -= speed * (nowtime - time_obstmove);

		printicon(road, roadx[0], 960 / 4 * 3 + 80);
		printicon(road, roadx[1], 960 / 4 * 3 + 80);

		if (roadx[0] < -2404)
		{
			swap(roadx[0], roadx[1]);
			roadx[1] = roadx[0] + 2404;
		}

		score = min(score + (nowtime - time_obstmove) * 0.008, 100000.0);
		time_obstmove = nowtime;

		for (; obst.size() && obst[0].x < -50; cloudnum -= (int)(obst[0].get_icon(0).iconbuf == cloud), obst.erase(obst.begin()), lastcacts--);
	}
}

// ��ӡ����
void printscore(double dscore, double dhighest)
{
	static int twinkle = 0, tktime;

	int score = (int)dscore, highest = (int)dhighest;

	if (highest)
	{
		string str_hiscore = int2string(highest);
		for (; str_hiscore.size() < 5; str_hiscore = "0" + str_hiscore);
		str_hiscore = "HI " + str_hiscore;
		outtextxy(1600 - 3 * 30 - str_hiscore.size() * 20, 0, str_hiscore.c_str());
	}

	// ���ٷ�����˸
	if (score % 100 == 0 && score)
	{
		twinkle = 4;
		tktime = GetTickCount();
	}
	if (GetTickCount() - tktime >= 250 && twinkle)
	{
		twinkle--;
		tktime = GetTickCount();
	}
	if (twinkle % 2)
		return;

	score = twinkle ? score / 100 * 100 : score;

	string str_score = int2string(score);
	for (; str_score.size() < 5; str_score = "0" + str_score);
	outtextxy(1600 - str_score.size() * 20, 0, str_score.c_str());
}

// ƽ�����ɰ����ҹ
void change_day_night(bool &changing, bool &atnight)
{
	static DWORD time_lastchange;
	if (!changing)
		time_lastchange = GetTickCount();

	changing = true;

	for (; time_lastchange < GetTickCount(); time_lastchange += 5)
	{
		for (int i = 0; i < 20; i++)
			daynight_needchange[i][1] = RGB((daynight_needchange[i][1] & 0xff) + ((atnight) ? -1 : 1), \
											(daynight_needchange[i][1] & 0xff) + ((atnight) ? -1 : 1), \
											(daynight_needchange[i][1] & 0xff) + ((atnight) ? -1 : 1));

		road[1] = daynight_needchange[0][1] - 1;

		setbkcolor(RGB(bkcor, bkcor, bkcor));
		settextcolor(RGB(0xff + 0x53 - bkcor, 0xff + 0x53 - bkcor, 0xff + 0x53 - bkcor));
		bkcor += ((!atnight) ? -1 : 1);

		if ((atnight && daynight_needchange[0][1] <= 0x535353) || ((!atnight) && daynight_needchange[0][1] >= 0xffffff))
		{
			changing = false;

			for (int i = 0; i < 20; i++)
				daynight_needchange[i][1] = (atnight) ? 0x535353 : 0xffffff;
			road[1] = (atnight) ? 0x535353 : 0xffffff;

			setbkcolor((atnight) ? 0xffffff : 0x535353);
			settextcolor((atnight) ? 0x535353 : 0xffffff);

			bkcor = (atnight) ? 0xff : 0x53;
			atnight = !atnight;
			return;
		}
	}
}

void game_over(int ofstx, int ofsty)
{
	printicon(dinosaur_die, ofstx, ofsty);
	printicon(replay, 1600 / 2 - 20, 960 / 2 - 20);
	outtextxy(1600 / 2 - 65, 960 / 2 + 50, "GAME  OVER");
	printicon(road, roadx[0], 960 / 4 * 3 + 80);
	printicon(road, roadx[1], 960 / 4 * 3 + 80);
	FlushBatchDraw();
}

int main()
{
	initgraph(1600, 960);

	// ����׼����������������
	init();

	double max_score = 0;

	while (1)
	{
		ingame = true;

		int dinosaur_ofstx = 1600 / 5, dinosaur_ofsty = 960 / 4 * 3;
		int fall_v = 0;// fall_v:�����ٶȣ�Ϊ����ʱ������
		int time_fall = GetTickCount(), time_start = GetTickCount();
		double speed = 0.4;// �ƶ��ٶȣ�ÿ�����������
		bool running = true, bend = false, inchanging = false, atnight = false;// running�����ܣ�Ҳ���ǲ�����, inchanging:���ڸı��ҹ������, atnignth:�Ƿ�������
		double score = 0;

		cloudnum = 0;
		time_obstmove = GetTickCount();

		ExMessage msg;

		vector<obstacle> obst;// ����ÿ���ϰ���

		while (ingame)// ��ʼʱ ingame Ϊ�٣����� enter ��ʼ��Ϸ 
		{
			cleardevice();

			// ���ƿ���
			draw_dinosaur_once(dinosaur_ofstx, dinosaur_ofsty, running, bend && (!fall_v));

			if (!ingame)
				break;

			// ������Ϣ
			if (peekmessage(&msg, EX_KEY))
			{
				unsigned char ch = msg.vkcode;
				if (msg.message == WM_KEYDOWN && (ch == VK_SPACE || ch == VK_UP) && \
					fall_v == 0 && dinosaur_ofsty >= 960 / 4 * 3 && !bend)// ��������������\����ʱ�������Ϊ
				{
					fall_v = -18;// ���������ٶ�
					time_fall = GetTickCount();
					running = false;
				}

				if (msg.message == WM_KEYDOWN && ch == VK_DOWN)// ���¡���
				{
					if (fall_v != 0 || dinosaur_ofsty != 960 / 4 * 3)// ��Ծ;�У�����ٵ���
						fall_v = 7;
					bend = true;
				}

				if (msg.message == WM_KEYUP && ch == VK_DOWN)// ̧�����
				{
					bend = false;
					dinosaur_ofstx = 1600 / 5, dinosaur_ofsty = 960 / 4 * 3;
				}
			}

			// ���¿�����Ծ������λ��
			dinosaur_jump_fall(dinosaur_ofstx, dinosaur_ofsty, fall_v, time_fall, running, bend);

			// ����ϰ���
			add_obstacle(obst, speed, score);

			// �ƶ��ϰ������·�� && ��ײ��� && ���·���
			move_and_collide(obst, speed, score);

			// ��ӡ����
			printscore(score, max_score);

			if (((int)score % 300 == 0 && (int)score) || inchanging)// �ı��ҹ������
				change_day_night(inchanging, atnight);
			FlushBatchDraw();
			speed = min(0.4 + (GetTickCount() - time_start) / 60000.0, 2.0);
		}

		cleardevice();
		for (DWORD i = 0; i < obst.size(); i++)
			obst[i].draw();
		printscore(score, max_score);
		game_over(dinosaur_ofstx + (bend ? 30 : 0), dinosaur_ofsty);

		setbkcolor(0xffffff);
		settextcolor(0x535353);
		bkcor = 0xff;

		atnight = false;
		inchanging = false;

		for (int i = 0; i < 20; i++)
			daynight_needchange[i][1] = 0x535353;
		road[1] = 0x535352;

		Sleep(500);

		flushmessage(EX_KEY);
		for (msg = getmessage(EX_KEY); msg.vkcode != VK_RETURN && msg.vkcode != VK_SPACE; msg = getmessage(EX_KEY));
		cleardevice();

		max_score = max(max_score, score);
	}
	return 0;
}
