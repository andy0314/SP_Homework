#include "status.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>

char statusPath[] = "player_status.txt";

char parent[] = "GGHHIIJJMMKNNLC";

char logBuf[150] = "";

int main (int argc, char* argv[]) {
	//TODO
	FILE* statusFile = fopen(statusPath, "r");
	Status playerStatus[8];
	for(int i = 0; i < 8; i++){
		playerStatus[i].real_player_id = i;
		char buf[10];
		fscanf(statusFile, "%d %d %s %c %d", &playerStatus[i].HP, &playerStatus[i].ATK, buf, &playerStatus[i].current_battle_id, &playerStatus[i].battle_ended_flag);
		if(buf[0] == 'F'){
			playerStatus[i].attr = FIRE;
		}
		else if(buf[0] == 'G'){
			playerStatus[i].attr = GRASS;
		}
		else{
			playerStatus[i].attr = WATER;
		}
	}
	fclose(statusFile);
	int currentId = atoi(argv[1]);
	int parentPid = atoi(argv[2]);
	int self_pid = getpid();
	if(currentId <= 7){
		//real player
		char logName[20];
		sprintf(logName, "log_player%d.txt", currentId);
		int logFd = open(logName, O_RDWR|O_CREAT|O_APPEND, 0644);
		Status currentPlayer = playerStatus[currentId];
		sprintf(logBuf, "%d,%d pipe to %c,%d %d,%d,%c,%d\n", currentId, self_pid, parent[currentId], parentPid, currentPlayer.real_player_id, currentPlayer.HP, currentPlayer.current_battle_id, currentPlayer.battle_ended_flag);
		write(logFd, logBuf, strlen(logBuf));
		write(1, &currentPlayer, sizeof(Status));
		while(true){
			read(0, &currentPlayer, sizeof(Status));
			sprintf(logBuf, "%d,%d pipe from %c,%d %d,%d,%c,%d\n", currentId, self_pid, parent[currentId], parentPid, currentPlayer.real_player_id, currentPlayer.HP, currentPlayer.current_battle_id, currentPlayer.battle_ended_flag);
			write(logFd, logBuf, strlen(logBuf));
			if(currentPlayer.battle_ended_flag == 1){
				if(currentPlayer.HP > 0){
					if(currentPlayer.current_battle_id != 'A'){
						//win
						currentPlayer.HP = (currentPlayer.HP + playerStatus[currentId].HP)/2;
						currentPlayer.battle_ended_flag = 0;
						sprintf(logBuf, "%d,%d pipe to %c,%d %d,%d,%c,%d\n", currentId, self_pid, parent[currentId], parentPid, currentPlayer.real_player_id, currentPlayer.HP, currentPlayer.current_battle_id, currentPlayer.battle_ended_flag);
						write(logFd, logBuf, strlen(logBuf));
						write(1, &currentPlayer, sizeof(Status));
					}
					else{
						//champion
						close(logFd);
						break;
					}
				}
				else{
					if(currentPlayer.current_battle_id == 'A'){
						close(logFd);
						break;
					}
					char fifoName[20];
					int agentId;
					if(currentPlayer.current_battle_id == 'G'){
						agentId = 8;
					}
					else if(currentPlayer.current_battle_id == 'I'){
						agentId = 9;
					}
					else if(currentPlayer.current_battle_id == 'D'){
						agentId = 10;
					}
					else if(currentPlayer.current_battle_id == 'H'){
						agentId = 11;
					}
					else if(currentPlayer.current_battle_id == 'J'){
						agentId = 12;
					}
					else if(currentPlayer.current_battle_id == 'E'){
						agentId = 13;
					}
					else if(currentPlayer.current_battle_id == 'B'){
						agentId = 14;
					}
					sprintf(fifoName, "player%d.fifo", agentId);
					sleep(1);
					int fifoFd = open(fifoName, O_RDWR);
					currentPlayer.HP = playerStatus[currentId].HP;
					currentPlayer.battle_ended_flag = 0;
					sprintf(logBuf, "%d,%d fifo to %d %d,%d\n", currentId, self_pid, agentId, currentPlayer.real_player_id, currentPlayer.HP);
					write(logFd, logBuf, strlen(logBuf));
					write(fifoFd, &currentPlayer, sizeof(Status));
					close(logFd);
					break;
				}
			}
			else{
				sprintf(logBuf, "%d,%d pipe to %c,%d %d,%d,%c,%d\n", currentId, self_pid, parent[currentId], parentPid, currentPlayer.real_player_id, currentPlayer.HP, currentPlayer.current_battle_id, currentPlayer.battle_ended_flag);
				write(logFd, logBuf, strlen(logBuf));
				write(1, &currentPlayer, sizeof(Status));
			}
		}
	}
	else{
		char fifoName[20];
		sprintf(fifoName, "player%d.fifo", currentId);
		mkfifo(fifoName, 0644);
		int fifoFd = open(fifoName, O_RDWR);
		Status PSSM;
		read(fifoFd, &PSSM, sizeof(Status));
		char logName[20];
		sprintf(logName, "log_player%d.txt", PSSM.real_player_id);
		int logFd = open(logName, O_RDWR|O_CREAT|O_APPEND, 0644);
		sprintf(logBuf, "%d,%d fifo from %d %d,%d\n", currentId, self_pid, PSSM.real_player_id,  PSSM.real_player_id, PSSM.HP);
		write(logFd, logBuf, strlen(logBuf));
		Status currentPlayer = PSSM;
		sprintf(logBuf, "%d,%d pipe to %c,%d %d,%d,%c,%d\n", currentId, self_pid, parent[currentId], parentPid, currentPlayer.real_player_id, currentPlayer.HP, currentPlayer.current_battle_id, currentPlayer.battle_ended_flag);
		write(logFd, logBuf, strlen(logBuf));
		write(1, &currentPlayer, sizeof(Status));
		//agent player
		while(true){
			read(0, &currentPlayer, sizeof(Status));
			sprintf(logBuf, "%d,%d pipe from %c,%d %d,%d,%c,%d\n", currentId, self_pid, parent[currentId], parentPid, currentPlayer.real_player_id, currentPlayer.HP, currentPlayer.current_battle_id, currentPlayer.battle_ended_flag);
			write(logFd, logBuf, strlen(logBuf));
			if(currentPlayer.battle_ended_flag == 1){
				if(currentPlayer.HP <= 0 || currentPlayer.current_battle_id == 'A'){
					close(logFd);
					break;
				}
				else{
					currentPlayer.HP = (currentPlayer.HP + playerStatus[currentPlayer.real_player_id].HP)/2;
					currentPlayer.battle_ended_flag = 0;
					sprintf(logBuf, "%d,%d pipe to %c,%d %d,%d,%c,%d\n", currentId, self_pid, parent[currentId], parentPid, currentPlayer.real_player_id, currentPlayer.HP, currentPlayer.current_battle_id, currentPlayer.battle_ended_flag);
					write(logFd, logBuf, strlen(logBuf));
					write(1, &currentPlayer, sizeof(Status));
				}
			}
			else{
				sprintf(logBuf, "%d,%d pipe to %c,%d %d,%d,%c,%d\n", currentId, self_pid, parent[currentId], parentPid, currentPlayer.real_player_id, currentPlayer.HP, currentPlayer.current_battle_id, currentPlayer.battle_ended_flag);
				write(logFd, logBuf, strlen(logBuf));
				write(1, &currentPlayer, sizeof(Status));
			}
		}
	}
	return 0;
}