#include "status.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef struct{
	char parent;
	char battle_id;
	char left;
	char right;
}battle;

//battle information
const battle btl[14] = {{0, 'A', 'B', 'C'}, {'A', 'B', 'D', 'E'}, {'A', 'C', 'F', 14}, {'B', 'D', 'G', 'H'}, {'B', 'E', 'I', 'J'}, {'C', 'F', 'K', 'L'}, {'D', 'G', 0, 1}, {'D', 'H', 2, 3}, {'E', 'I', 4, 5}, {'E', 'J', 6, 7}, {'F', 'K', 'M', 10}, {'F', 'L', 'N', 13}, {'K', 'M', 8, 9}, {'L', 'N', 11, 12}};
const Attribute field[14] = {FIRE, GRASS, WATER, WATER, FIRE, FIRE, FIRE, GRASS, WATER, GRASS, GRASS, GRASS, FIRE, WATER};

char logBuf[150] = "";

int main (int argc, char* argv[]) {
	//check battle id
	int parent_pid = atoi(argv[2]);
	int self_pid = getpid();
	battle currentBattle;
	Attribute currentField;
	bool findBattle = false;
	for(int i = 0; i < 14; i++){
		if(argv[1][0] == btl[i].battle_id){
			currentBattle = btl[i];
			currentField = field[i];
			findBattle = true;
			break;
		}
	}
	//set log file
	char logName[20];
	sprintf(logName, "log_battle%c.txt", currentBattle.battle_id);
	int logFd = open(logName, O_RDWR|O_CREAT|O_APPEND, 0644);
	//set pipe
	// 0: parent to child
	// 1: child to parent
	int leftPipe[2][2];
	int rightPipe[2][2];
	pipe(leftPipe[0]);
	pipe(leftPipe[1]);
	pipe(rightPipe[0]);
	pipe(rightPipe[1]);
	//fork
	int pidLeft = fork();
	if(pidLeft == 0){
		dup2(leftPipe[0][0], 0);
		dup2(leftPipe[1][1], 1);
		close(leftPipe[0][1]);
		close(leftPipe[1][0]);
		close(leftPipe[0][0]);
		close(leftPipe[1][1]);
		if(currentBattle.left >= 0 && currentBattle.left <= 14){			
			char leftName[] = "./player";
			char lArgva[30];
			char lArgvb[30];
			int ppid = getppid();
			sprintf(lArgva, "%d", currentBattle.left);
			sprintf(lArgvb, "%d", ppid);
			execlp(leftName, leftName, lArgva, lArgvb, NULL);
		}
		else{
			char leftName[] = "./battle";
			char lArgva[30];
			char lArgvb[30];
			int ppid = getppid();
			sprintf(lArgva, "%c", currentBattle.left);
			sprintf(lArgvb, "%d", ppid);
			execlp(leftName, leftName, lArgva, lArgvb, NULL);
		}
		exit(0);
	}
	int pidRight = fork();
	if(pidRight == 0){
		dup2(rightPipe[0][0], 0);
		dup2(rightPipe[1][1], 1);
		close(rightPipe[0][1]);
		close(rightPipe[1][0]);
		close(rightPipe[0][0]);
		close(rightPipe[1][1]);
		if(currentBattle.right >= 0 && currentBattle.right <= 14){
			char rightName[] = "./player";
			char rArgva[30];
			char rArgvb[30];
			int ppid = getppid();
			sprintf(rArgva, "%d", currentBattle.right);
			sprintf(rArgvb, "%d", ppid);
			execlp(rightName, rightName, rArgva, rArgvb, NULL);
		}
		else{
			char rightName[] = "./battle";
			char rArgva[30];
			char rArgvb[30];
			int ppid = getppid();
			sprintf(rArgva, "%c", currentBattle.right);
			sprintf(rArgvb, "%d", ppid);
			execlp(rightName, rightName, rArgva, rArgvb, NULL);
		}
		exit(0);
	}
	int winnerPipe[2];
	int winner;
	char winner_id;
	int winner_pid;
	//battle mode
	while(true){
		Status leftPlayer, rightPlayer;
		read(leftPipe[1][0], &leftPlayer, sizeof(Status));
		read(rightPipe[1][0], &rightPlayer, sizeof(Status));
		if(currentBattle.left >= 0 && currentBattle.left <= 14){
			sprintf(logBuf, "%c,%d pipe from %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftPlayer.real_player_id, leftPlayer.HP, leftPlayer.current_battle_id, leftPlayer.battle_ended_flag);
			write(logFd, logBuf, strlen(logBuf));
		}
		else{
			sprintf(logBuf, "%c,%d pipe from %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftPlayer.real_player_id, leftPlayer.HP, leftPlayer.current_battle_id, leftPlayer.battle_ended_flag);
			write(logFd, logBuf, strlen(logBuf));
		}
		if(currentBattle.right >= 0 && currentBattle.right <= 14){
			sprintf(logBuf, "%c,%d pipe from %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightPlayer.real_player_id, rightPlayer.HP, rightPlayer.current_battle_id, rightPlayer.battle_ended_flag);
			write(logFd, logBuf, strlen(logBuf));
		}
		else{
			sprintf(logBuf, "%c,%d pipe from %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightPlayer.real_player_id, rightPlayer.HP, rightPlayer.current_battle_id, rightPlayer.battle_ended_flag);
			write(logFd, logBuf, strlen(logBuf));
		}
		Status leftOutput = leftPlayer;
		Status rightOutput = rightPlayer;
		leftOutput.current_battle_id = currentBattle.battle_id;
		rightOutput.current_battle_id = currentBattle.battle_id;
		if(leftOutput.HP < rightOutput.HP || (leftOutput.HP == rightOutput.HP && leftOutput.real_player_id < rightOutput.real_player_id)){
			int damage = leftOutput.ATK;
			if(leftOutput.attr == currentField){
				damage = damage*2;
			}
			rightOutput.HP -= damage;
			if(rightOutput.HP > 0){
				damage = rightOutput.ATK;
				if(rightOutput.attr == currentField){
					damage = damage*2;
				}
				leftOutput.HP -= damage;
				if(leftOutput.HP > 0){
					if(currentBattle.left >= 0 && currentBattle.left <= 14){
						sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftOutput.real_player_id, leftOutput.HP, leftOutput.current_battle_id, leftOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					else{
						sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftOutput.real_player_id, leftOutput.HP, leftOutput.current_battle_id, leftOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					if(currentBattle.right >= 0 && currentBattle.right <= 14){
						sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightOutput.real_player_id, rightOutput.HP, rightOutput.current_battle_id, rightOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					else{
						sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightOutput.real_player_id, rightOutput.HP, rightOutput.current_battle_id, rightOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					write(leftPipe[0][1], &leftOutput, sizeof(Status));
					write(rightPipe[0][1], &rightOutput, sizeof(Status));
				}
				else{
					leftOutput.battle_ended_flag = 1;
					rightOutput.battle_ended_flag = 1;
					if(currentBattle.left >= 0 && currentBattle.left <= 14){
						sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftOutput.real_player_id, leftOutput.HP, leftOutput.current_battle_id, leftOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					else{
						sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftOutput.real_player_id, leftOutput.HP, leftOutput.current_battle_id, leftOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					if(currentBattle.right >= 0 && currentBattle.right <= 14){
						sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightOutput.real_player_id, rightOutput.HP, rightOutput.current_battle_id, rightOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					else{
						sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightOutput.real_player_id, rightOutput.HP, rightOutput.current_battle_id, rightOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					write(leftPipe[0][1], &leftOutput, sizeof(Status));
					write(rightPipe[0][1], &rightOutput, sizeof(Status));
					winner = rightOutput.real_player_id;
					winner_id = currentBattle.right;
					winner_pid = pidRight;
					winnerPipe[0] = rightPipe[1][0];
					winnerPipe[1] = rightPipe[0][1];
					waitpid(pidLeft, NULL, 0);
					break;
				}
			}
			else{
				leftOutput.battle_ended_flag = 1;
				rightOutput.battle_ended_flag = 1;
				if(currentBattle.left >= 0 && currentBattle.left <= 14){
					sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftOutput.real_player_id, leftOutput.HP, leftOutput.current_battle_id, leftOutput.battle_ended_flag);
					write(logFd, logBuf, strlen(logBuf));
				}
				else{
					sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftPlayer.real_player_id, leftOutput.HP, leftOutput.current_battle_id, leftOutput.battle_ended_flag);
					write(logFd, logBuf, strlen(logBuf));
				}
				if(currentBattle.right >= 0 && currentBattle.right <= 14){
					sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightOutput.real_player_id, rightOutput.HP, rightOutput.current_battle_id, rightOutput.battle_ended_flag);
					write(logFd, logBuf, strlen(logBuf));
				}
				else{
					sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightOutput.real_player_id, rightOutput.HP, rightOutput.current_battle_id, rightOutput.battle_ended_flag);
					write(logFd, logBuf, strlen(logBuf));
				}
				write(leftPipe[0][1], &leftOutput, sizeof(Status));
				write(rightPipe[0][1], &rightOutput, sizeof(Status));
				winner = leftOutput.real_player_id;
				winner_id = currentBattle.left;
				winner_pid = pidLeft;
				winnerPipe[0] = leftPipe[1][0];
				winnerPipe[1] = leftPipe[0][1];
				waitpid(pidRight, NULL, 0);
				break;
			}
		}
		else{
			int damage = rightOutput.ATK;
			if(rightOutput.attr == currentField){
				damage = damage*2;
			}
			leftOutput.HP -= damage;
			if(leftOutput.HP > 0){
				damage = leftOutput.ATK;
				if(leftOutput.attr == currentField){
					damage = damage*2;
				}
				rightOutput.HP -= damage;
				if(rightOutput.HP > 0){
					if(currentBattle.left >= 0 && currentBattle.left <= 14){
						sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftOutput.real_player_id, leftOutput.HP, leftOutput.current_battle_id, leftOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					else{
						sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftOutput.real_player_id, leftOutput.HP, leftOutput.current_battle_id, leftOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					if(currentBattle.right >= 0 && currentBattle.right <= 14){
						sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightOutput.real_player_id, rightOutput.HP, rightOutput.current_battle_id, rightOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					else{
						sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightOutput.real_player_id, rightOutput.HP, rightOutput.current_battle_id, rightOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					write(leftPipe[0][1], &leftOutput, sizeof(Status));
					write(rightPipe[0][1], &rightOutput, sizeof(Status));
				}
				else{
					leftOutput.battle_ended_flag = 1;
					rightOutput.battle_ended_flag = 1;
					if(currentBattle.left >= 0 && currentBattle.left <= 14){
						sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftOutput.real_player_id, leftOutput.HP, leftOutput.current_battle_id, leftOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					else{
						sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftOutput.real_player_id, leftOutput.HP, leftOutput.current_battle_id, leftOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					if(currentBattle.right >= 0 && currentBattle.right <= 14){
						sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightOutput.real_player_id, rightOutput.HP, rightOutput.current_battle_id, rightOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					else{
						sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightOutput.real_player_id, rightOutput.HP, rightOutput.current_battle_id, rightOutput.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
					}
					write(leftPipe[0][1], &leftOutput, sizeof(Status));
					write(rightPipe[0][1], &rightOutput, sizeof(Status));
					winner = leftOutput.real_player_id;
					winner_id = currentBattle.left;
					winner_pid = pidLeft;
					winnerPipe[0] = leftPipe[1][0];
					winnerPipe[1] = leftPipe[0][1];
					waitpid(pidRight, NULL, 0);
					break;
				}
			}
			else{
				leftOutput.battle_ended_flag = 1;
				rightOutput.battle_ended_flag = 1;
				if(currentBattle.left >= 0 && currentBattle.left <= 14){
					sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftOutput.real_player_id, leftOutput.HP, leftOutput.current_battle_id, leftOutput.battle_ended_flag);
					write(logFd, logBuf, strlen(logBuf));
				}
				else{
					sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.left, pidLeft, leftOutput.real_player_id, leftOutput.HP, leftOutput.current_battle_id, leftOutput.battle_ended_flag);
					write(logFd, logBuf, strlen(logBuf));
				}
				if(currentBattle.right >= 0 && currentBattle.right <= 14){
					sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightOutput.real_player_id, rightOutput.HP, rightOutput.current_battle_id, rightOutput.battle_ended_flag);
					write(logFd, logBuf, strlen(logBuf));
				}
				else{
					sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.right, pidRight, rightOutput.real_player_id, rightOutput.HP, rightOutput.current_battle_id, rightOutput.battle_ended_flag);
					write(logFd, logBuf, strlen(logBuf));
				}
				write(leftPipe[0][1], &leftOutput, sizeof(Status));
				write(rightPipe[0][1], &rightOutput, sizeof(Status));
				winner = rightOutput.real_player_id;
				winner_id = currentBattle.right;
				winner_pid = pidRight;
				winnerPipe[0] = rightPipe[1][0];
				winnerPipe[1] = rightPipe[0][1];
				waitpid(pidLeft, NULL, 0);
				break;
			}
		}
	}
	//passing mode
	if(currentBattle.parent == 0){
		//print winner
		printf("Champion is P%d\n", winner);
	}
	else{
		//pass status between parent and child
		while(true){
			Status winnerStatus;
			read(winnerPipe[0], &winnerStatus, sizeof(Status));
			if(winner_id >= 0 && winner_id <= 14){
				sprintf(logBuf, "%c,%d pipe from %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, winner_id, winner_pid, winnerStatus.real_player_id, winnerStatus.HP, winnerStatus.current_battle_id, winnerStatus.battle_ended_flag);
				write(logFd, logBuf, strlen(logBuf));
			}
			else{
				sprintf(logBuf, "%c,%d pipe from %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, winner_id, winner_pid, winnerStatus.real_player_id, winnerStatus.HP, winnerStatus.current_battle_id, winnerStatus.battle_ended_flag);
				write(logFd, logBuf, strlen(logBuf));
			}
			sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.parent, parent_pid, winnerStatus.real_player_id, winnerStatus.HP, winnerStatus.current_battle_id, winnerStatus.battle_ended_flag);
			write(logFd, logBuf, strlen(logBuf));
			write(1, &winnerStatus, sizeof(Status));
			read(0, &winnerStatus, sizeof(Status));
			sprintf(logBuf, "%c,%d pipe from %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, currentBattle.parent, parent_pid, winnerStatus.real_player_id, winnerStatus.HP, winnerStatus.current_battle_id, winnerStatus.battle_ended_flag);
			write(logFd, logBuf, strlen(logBuf));
			if(winner_id >= 0 && winner_id <= 14){
				sprintf(logBuf, "%c,%d pipe to %d,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, winner_id, winner_pid, winnerStatus.real_player_id, winnerStatus.HP, winnerStatus.current_battle_id, winnerStatus.battle_ended_flag);
				write(logFd, logBuf, strlen(logBuf));
			}
			else{
				sprintf(logBuf, "%c,%d pipe to %c,%d %d,%d,%c,%d\n", currentBattle.battle_id, self_pid, winner_id, winner_pid, winnerStatus.real_player_id, winnerStatus.HP, winnerStatus.current_battle_id, winnerStatus.battle_ended_flag);
				write(logFd, logBuf, strlen(logBuf));
			}
			if(winnerStatus.battle_ended_flag == 1 && winnerStatus.HP <= 0){
				write(winnerPipe[1], &winnerStatus, sizeof(Status));
				break;
			}
			else if(winnerStatus.battle_ended_flag == 1 && winnerStatus.current_battle_id == 'A'){
				write(winnerPipe[1], &winnerStatus, sizeof(Status));
				break;
			}
			else{
				write(winnerPipe[1], &winnerStatus, sizeof(Status));
			}
		}
	}
	wait(NULL);
	close(logFd);
	return 0;
}