#include <iostream>
#include <vector>
#include <string>

using namespace std;

class AI {
public:
	AI();
	string board;
	int move;
	int findNewMove();
	void printBoard();
	void aiPlay();
	void oppPlay();
	void receiveBoard(string newBoard);
};

AI::AI() {
	board = ".........";
}

void AI::receiveBoard(string newBoard) {
	board = newBoard;
}


void AI::aiPlay() {

	move = findNewMove();
	if(board[move] != '.') {
		cout << "spot taken. lose a turn" << endl;
		return;
	}

	board[move] = 'R';

}

void AI::oppPlay() {

	int oppMove;

	cout << "Enter move: ";
	cin >> oppMove;

	if(board[oppMove] != '.') {
		cout << "spot taken. lose a turn" << endl;
		return;
	}

	board[oppMove] = 'G';

}




void AI::printBoard() {

	if (board.size() != 9) {
		cout << "board size is " << board.size() << endl;
		return;
	}

	for (int i = 0; i < 9; i++) {
		cout << board[i] << ' ';
		if(i%3 == 2) {
			cout << endl;
		}
	}
}

int AI::findNewMove() {
	// extremely dumb downed version of ttt ai
	for (int i = 0; i < 9; i++) {
		if (board[i] == '.') {
			return i;
		}
	}

	return -1;
}

int main() {

	AI A;
	A.printBoard();

	cout << "game starts" << endl;
	while(1) {
		A.aiPlay();
		cout << "Computer played a move" << endl;
		A.printBoard();

		// The following three lines will be replaced by receiving a new board
		A.oppPlay();
		cout << "Opponent played a move" << endl;
		A.printBoard();
	}

	return 0;
}