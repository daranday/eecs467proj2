#include <iostream>
#include <vector>
#include <string>
#include <utility> 

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
    bool isBoardEmpty();
    int twoD2oneD(int x, int y);
    pair<int, int> oneD2twoD(int x);
    bool isValidCoord2D(int x, int y);
    bool checkEnd();

};

AI::AI() {
    board = ".........";
}

// returns 0 if winner not determined, 1 if red wins, 2 if green wins, 3 if tie
bool AI::checkEnd() {
    int wins[8][3] = {{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};

    int redCount, greenCount;
    bool emptyCell = false;

    for(int i = 0; i < 8; i++) {
        redCount = 0;
        greenCount = 0;
        for (int j = 0; j < 3; j++) {
            if(board[wins[i][j]] == 'R') redCount++;
            if(board[wins[i][j]] == 'G') greenCount++;
            if(board[wins[i][j]] == '.') emptyCell = true;
        }

        if(redCount == 3) {
            cout << "Red Wins!" << endl;
            return true;
        } else if(greenCount == 3) {
            cout << "Green Wins!" << endl;
            return true;
        }
    }

    if(emptyCell) {
        return false;
    } else {
        cout << "It's a tie!" << endl;
        return true;
    }
}

int AI::twoD2oneD(int x, int y) {
    return y*3 + x;
}

pair<int, int> AI::oneD2twoD(int x) {
    return pair<int, int> (x % 3, x/3);
}

bool AI::isValidCoord2D(int x, int y) {
    if (x < 0 || x > 2 || y < 0 || y > 2) {
        return false;
    } else {
        return true;
    }
}

bool AI::isBoardEmpty() {
    for(int i = 0; i < 9; i++) {
        if (board[i] != '.') {
            return false;
        }
    }

    return true;
}

void AI::receiveBoard(string newBoard) {
    board = newBoard;
}


void AI::aiPlay() {

    move = findNewMove();
    cout << "Computer played: " << move << endl;
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

    if(isBoardEmpty()) {
        cout << "board is empty" << endl;
        return 4;
    }

    int wins[8][3] = {{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};

    int move;
    int count;
    bool validMove;

    // having 2 red balls on a line (one more move to win)
    for(int i = 0; i < 8; i++) {
        count = 0;
        validMove = false;
        for(int j = 0; j < 3; j++) {
            if(board[wins[i][j]] == 'R') {
                count++;
            } else if (board[wins[i][j]] == '.') {
                validMove = true;
                move = wins[i][j];
            }
        }
        if(count == 2 && validMove) {
            return move;
        }
    }

    // having 2 green balls on a line (about to lose)
    for(int i = 0; i < 8; i++) {
        count = 0;
        validMove = false;
        for(int j = 0; j < 3; j++) {
            if(board[wins[i][j]] == 'G') {
                count++;
            } else if (board[wins[i][j]] == '.') {
                validMove = true;
                move = wins[i][j];
            }
        }
        if(count == 2 && validMove) {
            return move;
        }
    }

    int maxSoFar = 0;
    int adjCount;
    pair<int, int> coord2D;

    int coord1D;

    int emptyCount, adjCount1, adjCount2;
    pair<int, int> coord2D1;
    pair<int, int> coord2D2;

    for(int i = 0; i < 8; i++) {
        count = 0;
        emptyCount = 0;
        validMove = false;
        for(int j = 0; j < 3; j++) {
            if(board[wins[i][j]] == 'R') {
                move = j;
                count++;
            } else if (board[wins[i][j]] == '.') {
                emptyCount++;
            }
        }

            if(count == 1 && emptyCount == 2) {
                if(move == 0 || move == 2) {
                    cout << "here" << endl;
                    return wins[i][1];
                }
                if(move == 1) {
                    coord2D1 = oneD2twoD(wins[i][0]);
                    coord2D2 = oneD2twoD(wins[i][2]);

                    adjCount1 = 0;
                    adjCount2 = 0;
                    for (int j = -1; j < 1; j++) {
                        for (int k = -1; k < 1; k++) {
                            if (j != 0 && k != 0) {
                                if(isValidCoord2D(coord2D1.first + j, coord2D1.second + k)) {
                                    if(board[twoD2oneD(coord2D1.first + j, coord2D1.second + k)] == 'R') {
                                        adjCount1++;
                                    }

                                    if(board[twoD2oneD(coord2D2.first + j, coord2D2.second + k)] == 'R') {
                                        adjCount2++;
                                    }

                                }

                            }
                        }
                    }

                    if(adjCount2 > adjCount1) {
                        return wins[i][2];
                    } else {
                        return wins[i][0];
                    }

                }
            }
        
    }

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
        A.checkEnd();
        A.aiPlay();
        cout << "Computer played a move" << endl;
        A.printBoard();
        A.checkEnd();

        // The following three lines will be replaced by receiving a new board
        A.oppPlay();
        cout << "Opponent played a move" << endl;
        A.printBoard();
    }

    return 0;
}