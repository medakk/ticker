/*
ticker v0.1
-Karthik Karanth (c)2015
*/

/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <ncurses.h>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <csignal>
#include <cstdlib>

///////////////////////////////////////
// data structures
///////////////////////////////////////
struct Timesig { //represents time signature in the form num / den
	int num, den;
};

///////////////////////////////////////
// function prototypes
///////////////////////////////////////
void displayHelp();
void resize(int sig);
float getWaitTime(float tempo, Timesig & timesig); //returns the interval between successive ticks
float getTempo(float waitTime, Timesig & timesig);
void drawFilledBox(int x, int y, int w, int h);
void drawEmptyBox(int x, int y, int w, int h);

///////////////////////////////////////
// global variables
///////////////////////////////////////
int width, height, startX, startY; //width/height of each tick's cell
float tempo = 80.0;
Timesig timesig = {4,4};

///////////////////////////////////////
// constants
///////////////////////////////////////
const int GAP = 2;

const int HIGHLIGHT_COLOR = 1;
const int NORMAL_COLOR = 2;
const int BORDER_COLOR = 3;

int main(int argc, char** argv) {

	//parse command line arguments
	if(argc > 1) {
		int i = 1; //argv[0] is the executable's name
		while(i < argc) {
			if(strcmp(argv[i], "-h") == 0) {
				displayHelp();
				return 0;
			} else if(strcmp(argv[i], "-t") == 0) {
				i++;
				
				if(i>=argc) {
					std::cerr << argv[0] << ": No tempo entered\n";
					return 1;
				}
				
				char *err;
				tempo = strtof(argv[i], &err);
				if(strlen(err)) {
					std::cerr << argv[0] << ": Invalid tempo: " << argv[i] << std::endl;
					return 1;
				}
			} else if(strcmp(argv[i], "-s") == 0) {
				i++;
				if(i>=argc) {
					std::cerr << argv[0] << ": Invalid time signature: " << argv[i] << std::endl;
					return 2;
				}
				
				char *err;
				timesig.num = (int) strtol(argv[i], &err, 10);
				if(strlen(err)) {
					std::cerr << argv[0] << ": Invalid time signature: " << argv[i] << std::endl;
					return 2;
				}
				
				i++;
				if(i>=argc) {
					std::cerr << argv[0] << ": Invalid time signature: " << argv[i] << std::endl;
					return 2;
				}
				
				timesig.den = (int) strtol(argv[i], &err, 10);
				if(strlen(err)) {
					std::cerr << argv[0] << ": Invalid time signature: " << argv[i] << std::endl;
					return 2;
				}
			} else {
				std::cerr << argv[0] << ": Unrecognized option: " << argv[i] << std::endl;
				return 3;
			}
			
			i++;
		}
	}
	
	//initialize curses
	initscr();
	noecho();
	curs_set(0); //hide the cursor
	nodelay(stdscr, true);
	
	start_color();
	init_pair(HIGHLIGHT_COLOR, COLOR_WHITE, COLOR_GREEN);
	init_pair(NORMAL_COLOR, COLOR_WHITE, COLOR_RED);
	init_pair(BORDER_COLOR, COLOR_BLACK, COLOR_WHITE);
	
	float waitTime = getWaitTime(tempo, timesig);
	
	timeval tv;
	gettimeofday(&tv, NULL); //NULL represents the time zone, it defaults to the locale's default
	double initUpdateTime = tv.tv_sec + (tv.tv_usec / 1000000.0);
	double nextUpdateTime = waitTime;
	double currentUpdateTime;
	
	//resize sets the width and height
	resize(0);
	
	int attr;
	int tickPosition = 1;
	int x = startX + (tickPosition-1) * (GAP+width);
	
	//tell curses to call resize whenever the window is resized
	signal(SIGWINCH, resize);
	
	bool paused = false;
	
	//used to setup the "tap tempo" feature
	float lastSpacePress = 0;
	
	while(true) {
		// input
		char c = getch();
		switch(c) {
			//quit
			case 'q':
				endwin();
				return 0;
			
			//increase tempo
			case '+':
			case '=':
				tempo += 1;
				waitTime = getWaitTime(tempo, timesig);
				tickPosition = 1;
				nextUpdateTime = currentUpdateTime + waitTime;
				break;
			//decrease tempo
			case '-':
				tempo -= 1;
				waitTime = getWaitTime(tempo, timesig);
				tickPosition = 1;
				nextUpdateTime = currentUpdateTime + waitTime;
				break;
				
			//tap tempo
			case ' ':
				waitTime = currentUpdateTime - lastSpacePress;
				tempo = getTempo(waitTime, timesig);
				lastSpacePress = currentUpdateTime;
				nextUpdateTime = currentUpdateTime + waitTime;
				tickPosition = 1;				
				break;
			
			case 'p':
				if(paused) {					
					paused = false;
				} else {
					paused = true;
				}
				break;
		}
	
		// update
		if(!paused) {
			gettimeofday(&tv, NULL);
			currentUpdateTime = tv.tv_sec + (tv.tv_usec / 1000000.0) - initUpdateTime;
		
			if(currentUpdateTime >= nextUpdateTime) {			
				tickPosition++;
				if(tickPosition > timesig.num) {
					tickPosition = 1;
				}						
				nextUpdateTime = currentUpdateTime + waitTime;
			}
		}

		// draw
		clear();
		
		if(tickPosition == 1) {
			attr = COLOR_PAIR(HIGHLIGHT_COLOR) | A_BOLD;
		} else {
			attr = COLOR_PAIR(NORMAL_COLOR);
		}
		x = startX + (tickPosition-1) * (GAP+width);
		attron(COLOR_PAIR(BORDER_COLOR));
		drawEmptyBox(x, startY, width, height);		
		attroff(COLOR_PAIR(BORDER_COLOR));
		
		attron(attr);		
		drawFilledBox(x+1, startY+1, width-2, height-2);	
		attroff(attr);
		
		mvprintw(LINES-1, COLS-45, "%1.1f bpm, %d/%d signature, %1.2fs elapsed", tempo, timesig.num, timesig.den, currentUpdateTime);
		
		refresh();
		
		// sleep
		usleep(0.018 * 1000000); //assuming fixed update rate of 60Hz
	}

	return 0;
}

void displayHelp() {
	using namespace std;
	cout 
		<< "Usage: ticker [options]" << endl
		<< "Options:\n"
		<< "  -h          show this help message\n"
		<< "  -t <tempo>  set the tempo\n"
		<< "  -s <a> <b>  set the time signature to a/b" << endl;
}

void resize(int sig) {
	endwin();
	refresh();
	clear();
	
	width = (COLS - (GAP * (timesig.num+1))) / timesig.num;
	height = LINES - (GAP*3);
	startX = GAP;
	startY = GAP;
}

float getWaitTime(float tempo, Timesig & timesig) {
	//assumes bpm corresponds to quarter notes
	//return (60.0/tempo) * (4.0 / timesig.den);
	return 240.0/(tempo*timesig.den);
}

float getTempo(float waitTime, Timesig & timesig) {
	return 240.0/(waitTime*timesig.den);
}

void drawFilledBox(int x, int y, int w, int h) {
	for(int j = y; j<=(y+h); j++) {
		mvhline(j,x,' ',w+1);
	}
}

void drawEmptyBox(int x, int y, int w, int h) {
	mvvline(y, x, ' ', h);
	mvvline(y, x+w, ' ', h);
	mvhline(y,x, ' ', w);
	mvhline(y+h,x, ' ', w+1); //don't know why +1 is required
}
