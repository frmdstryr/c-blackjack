/*
 * blackjack.c
 *
 *  Created on: Mar 1, 2014
 *      Author: jrm
 *
 *  Specification
 *      You are to write a multi-player blackjack program. The program must enforce a maximum of six
 *      players. The program must make use of unnamed pipes. The parent process will be the dealer.
 *      The children processes will be the players. The game continues until there are no players left. To
 *      leave the game a player bets $0. When a player enters the game, they will tell the dealer the
 *      amount of money they will use to begin the game.
 *      The program must implement signal handlers for the termination (SIGINT) and stop (SIGTSTP)
 *      signals. If the program receives the termination signal (because all players left the game or from
 *      the keyboard), the user should be prompted to shutdown the game or start a new game. If the
 *      program receives the stop signal and there are no players left, the program should end gracefully;
 *      if there are players still in the game, the signal should be ignored.
 *
 *      The following will be processes of your program:
 *      * Dealer process
 *        - Shuffle the cards
 *        - Get initial bets from players
 *        - Deal the cards to the players and the dealer using a random-number generator
 *        - Determine which player(s) wins
 *      * Players processes (one child process for each player)
 *        - Ask the user how much they want to bet
 *        - Get cards from dealer and tell user
 *        - Repeat until player stays
 *            * Ask the user if they want to stay or hit (take a card)
 *            * Tell the dealer decision (stay or hit)
 *
 *   Usage Clause:
 *      blackjack <number_of_players>
 *
 *   Notes
 *     - Do not implement the blackjack concepts of double-down or splitting.
 *     - All players should be able to view the dealer’s hand.
 *     - Any non-reentrant function that is interrupted by a signal must be restarted.
 */
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include "restart.h"
#include "helpers.h"


// ========================================= CARD =================================================

// I guess this is irrelevant to blackjack but whatever
#ifndef Suite
typedef enum { CLUBS,DIAMONDS,HEARTS,SPADES } Suite;
#endif

#ifndef Card
/**
 * Representation of a card
 */
typedef struct Card {
	Suite suite; // just for kicks
	char *name;
	int values[2]; // If score over 21 use value 2
} Card;
Card *card_create(char *name,Suite suite,int val0,int val1);
bool card_destroy(Card *card);
bool card_is_ace(Card *card);
char *card_to_str(Card *card);
#endif

Card *card_create(char *name,Suite suite,int val0,int val1) {
	Card *card = malloc(sizeof(Card));
	assert(card != NULL);
	card->name = name;
	card->suite = suite;
	card->values[0] = val0;
	card->values[1] = val1;
	return card;
}

char *card_to_str(Card *card) {
	char *str;
	//char *suites[4] = {"Clubs","Diamonds","Hearts","Spades"};
	//assert(asprintf(&str,"Card(name=%s,suite=%s,values=[%i,%i])",card->name,suites[card->suite],card->values[0],card->values[1])>0);
	char *suites[4] = {"C","D","H","S"};
	assert(asprintf(&str,"%s%s",card->name,suites[card->suite])>0);
	return str;
}

bool card_is_ace(Card *card) {
	if (strcmp(card->name,"A")==0) {
		return TRUE;
	}
	return FALSE;
}

bool card_destroy(Card *card) {
	assert(card!=NULL);
	free(card->name);
	free(card);
	return TRUE;
}

// ========================================= CARD =================================================

// ========================================= PLAYER =================================================

#ifndef Player
/**
 * Representation of a Player
 */
typedef struct Player {
	int id; // index in player array
	Card *cards[21]; // Cards the player has, could have up to 21
	int _num_cards;
	int _num_games;
	int score;
	double money;
	double bet;
	bool busted;
} Player;
Player *player_create(int id);
bool player_destroy(Player *player);
bool player_init_round(Player *player);
char *player_cards_to_str(Player *player);
char *player_to_str(Player *player);
bool player_hit(Player *player,Card *card);
int player_cmp(Player *player,Player *dealer);
#endif

Player *player_create(int id) {
	Player *player = malloc(sizeof(Player));
	assert(player != NULL);
	player->id = id;
	player->_num_cards = 0;
	player->score = 0;
	player->money = 0.0;
	player->bet = 0.0;
	player->busted = FALSE;
	player->_num_games = 0;
	return player;
}

/**
 * Resets the player so a new game can be played
 */
bool player_init_round(Player *player) {
	player->busted = FALSE;
	player->score = 0;
	player->bet = 0;

	// Clear any cards they have
	while(player->_num_cards>0) {
		player->_num_cards--;
		player->cards[player->_num_cards] = NULL;
	}

	return TRUE;
}

char *player_cards_to_str(Player *player) {
	// Print the cards
	int i;
	char *buf = "[";
	for (i=0;i<player->_num_cards;i++) {
		asprintf(&buf,"%s%s",buf,card_to_str(player->cards[i]));
		if(i!=player->_num_cards-1) {asprintf(&buf,"%s,",buf);}
	}
	assert(asprintf(&buf,"%s]",buf)>0);
	return buf;
}

char *player_to_str(Player *player) {
	char *str;
	char *bools[2] = {"False","True"};
	assert(asprintf(&str,"Player(id=%i,score=%i,money=%0.2f,busted=%s,cards=%s)",player->id,player->score,player->money,bools[player->busted],player_cards_to_str(player))>0);
	return str;
}

bool player_destroy(Player *player) {
	assert(player!=NULL);
//	int i=0;
//	for (i=0;i<player->_num_cards;i++) {
//		assert(card_destroy(player->cards[i]));
//	}
	free(player);
	return TRUE;
}

/**
 * When a player chooses 'hit'
 * 1. Add a card to the players set
 * 2. Updates their score
 * 3. Set the busted flag (if applicable).
 */
bool player_hit(Player *player,Card *card) {
	int i= 0;
	int j = 0;
	player->score = 0;
	int num_aces = 0;

	// Cannot hit, they already busted
	if (player->busted) {
		return FALSE;
	}

	// Get another card
	player->cards[player->_num_cards] = card;
	player->_num_cards ++;

	// Calculate score and set busted flag
	for (i=0;i<player->_num_cards;i++){
		player->score += player->cards[i]->values[0];
	}

	// If their score is over 21 and they have
	// an ace, recalculate the score
	num_aces = player_count_aces(player);
	if (player->score>21 && num_aces>0) {
		for(j=1;j<num_aces && player->score>21;j++) { // while still busted
			player->score = 0; // reset
			int aces_used = 0;

			// recalculate score
			for (i=0;i<player->_num_cards;i++){
				if (card_is_ace(player->cards[i]) && aces_used<j) {
					aces_used++;
					player->score += player->cards[i]->values[1];
				} else {
					player->score += player->cards[i]->values[0];
				}
			}
		}
	}

	if (player->score>21) {
		player->busted = TRUE;
	}
	return TRUE;
}

/**
 * Returns the number of aces the player has
 */
int player_count_aces(Player *player) {
	int i;
	int num_aces=0;
	for (i=0;i<player->_num_cards;i++) {
		if (card_is_ace(player->cards[i])) {
			num_aces++;
		}
	}
	return num_aces;
}

/**
 * Make a bet and subtract from money left
 * 	 - Minimum bet is $5
 * 	 - Maximum bet is the amount the player has left
 * 	 - Bets are in multiples of $5
 * returns TRUE if the bet was valid and made.
 */
bool player_bet(Player *player,double amount) {
	if (amount<5 || fmod(amount,5.0)!=0.0) {
		printf("Minimum bet is $5 and must be in $5 increments!\n");
		return FALSE;
	}
	if (player->money>=amount) { // if they have enough
		player->bet = amount;
		player->money -= amount;
		return TRUE;
	}
	printf("You don't have enough money to bet that much!\n");
	return FALSE;
}

/**
 * Returns 1 if the player beat the dealer, 0 if tie, -1 if lost.
 */
int player_cmp(Player *player,Player *dealer) {
	if (player->busted) {
		return -1;
	} else if (dealer->busted || player->score>dealer->score) {
		return 1; // player didn't bust and score is higher
	} else if (player->score==dealer->score){
		return 0; // tie
	}
	// lost
	return -1;
}
// ========================================= PLAYER =================================================

// ========================================= BLACKJACK =================================================
#ifndef Blackjack
/**
 * Representation of game state
 */
typedef struct Blackjack {
	Card *deck[52]; // Cards still left in the deck
	int _num_cards; // Cards left in the deck
	Player *players[14]; // I guess you could have more...
	int _num_players; // Players
	bool finished;
} Blackjack;
Blackjack *blackjack_create(int num_players);
bool blackjack_init_round(Blackjack *game);
bool blackjack_init_deck(Blackjack *game);
bool blackjack_shuffle_deck(Blackjack *game);
bool blackjack_deal_card(Blackjack *game, Player *player);
#endif

/**
 * Creates the blackjack and inits the players
 * and game.
 */
Blackjack *blackjack_create(int num_players) {
	Blackjack *game = malloc(sizeof(Blackjack));
	assert(game != NULL);
	srand(time(NULL));
	int i;

	// Init vals
	game->_num_players=0;
	game->_num_cards=0;
	game->finished=FALSE;

	// Init players
	for (i=0;i<num_players;i++) {
		game->players[i] = player_create(i);
		game->_num_players++;
	}

	return game;
}

/**
 * Restarts a game by:
 * 1. Collecting all the cards from the players
 * 2. Re-shuffling the deck
 * 3. Dealing cards to all the players
 */
bool blackjack_init_round(Blackjack *game) {
	int i;
	game->finished = FALSE;

	// Create the deck
	assert(blackjack_init_deck(game));

	// Shuffle the deck
	for (i=0;i<7;i++) {
		assert(blackjack_shuffle_deck(game));
	}

	// Clear any cards the players have
	for (i=0;i<game->_num_players;i++) {
		assert(player_init_round(game->players[i]));
	}

	// Deal cards out to the players
	for (i=0;i<(game->_num_players);i++) {
		assert(blackjack_deal_card(game,game->players[i]));
		assert(blackjack_deal_card(game,game->players[i]));
	}

	return TRUE;
}

/**
 * Create the deck
 */
bool blackjack_init_deck(Blackjack *game) {
	char *c[11] = {"0","1","2","3","4","5","6","7","8","9","10"};
	game->_num_cards=0;
	int i,j;
	//CardSuite suite = CLUBS;
	for (j=0;j<4;j++) { // foreach suite
		for (i=2;i<11;i++) { // 1-9
			game->deck[game->_num_cards] = card_create(c[i],j,i,i);
			game->_num_cards++;
		}

		game->deck[game->_num_cards] = card_create("J",j,10,10);
		game->_num_cards++;
		game->deck[game->_num_cards] = card_create("Q",j,10,10);
		game->_num_cards++;
		game->deck[game->_num_cards] = card_create("K",j,10,10);
		game->_num_cards++;
		game->deck[game->_num_cards] = card_create("A",j,11,1);
		game->_num_cards++;
	}
	return TRUE;
}

/**
 * Shuffle the cards by randomly sorting the indexes of the cards
 * based on: http://stackoverflow.com/questions/6127503/shuffle-array-in-c
 */
bool blackjack_shuffle_deck(Blackjack *game) {
	assert(game->_num_cards>0);
	Card *tmp;
	int i;
	for (i=0;i<game->_num_cards-1;i++) {
		// Pick a random index
		int j = i + rand() / (RAND_MAX / (game->_num_cards - i) + 1);
		// Swap the two cards
		tmp = game->deck[j];
		game->deck[j] = game->deck[i];
		game->deck[i] = tmp;
	}
	return TRUE;
}

/**
 * Takes a card from the top of the deck and gives it to the player.
 */
bool blackjack_deal_card(Blackjack *game, Player *player) {
	assert(game->_num_cards>0);
	game->_num_cards--;
	assert(player_hit(player,game->deck[game->_num_cards]));
	game->deck[game->_num_cards] = NULL; // Remove this card from the deck
	return TRUE;
}

/**
 * Remove a player from the game. If iterating over players
 * make sure to decrement your count after removing!
 */
bool blackjack_remove_player(Blackjack *game,Player *player) {
	int i=0;
	bool found=FALSE;
	if (game->_num_players<1) { // Can't remove the dealer
		return FALSE;
	}

	// Shift all the items in the array back one after found
	for (i=0;i<game->_num_players;i++) {
		if (game->players[i]==player) {
			assert(player_destroy(player));
			game->players[i] = NULL;
			found = TRUE;
		} else if (found) {
			game->players[i-1] = game->players[i];
		}
	}

	if (found) {
		game->_num_players--;
	}
	return found;
}
// ========================================= BLACKJACK =================================================


// ========================================= CLIENT =================================================
#ifndef Client
/**
 * An interface to communicate via pipes
 */
typedef struct Client {
	int id;
	pid_t pid;
	int sfd[2]; // pipe fds for stdout
	int rfd[2]; // pipe fds for read
	int wfd[2]; // pipe fds for write
} Client;
Client *client_create(int id);
bool client_close(Client *client);
int client_main();
bool client_destroy(Client *client);
int client_printf(Client *client,const char *fmt,...);
int client_sendline(Client *client,const char *fmt,...);
char *client_readline(Client *client);
#endif

Client *client_create(int id) {
	Client *client = malloc(sizeof(Client));
	assert(client != NULL);

	client->id = id;

	// Create the pipes
	assert(pipe(client->rfd) !=-1); // for read
	assert(pipe(client->wfd) !=-1); // for read

	// Fork the process
	client->pid = fork();
	assert(client->pid != -1);

	// If child, do client_main()
	if (client->pid==0) {
		assert(r_close(client->rfd[0])!=-1); // client writes to rfd1, close 0
		assert(r_close(client->wfd[1])!=-1); // client reads from wrf0, close 1;

		client_main(client);
	} else {
		assert(r_close(client->rfd[1])!=-1); // parent reads from rfd0, close 1
		assert(r_close(client->wfd[0])!=-1); // parent writes to wfd1, close 0
	}

	// Return object client to parent;
	return client;
}

/**
 * Send a line to the child, if used in the child process
 * it sends a line to the parent.
 */
int client_sendline(Client *client,const char *fmt,...) {
	int fd = client->wfd[1]; // parent
	if (client->pid==0) {
		fd=client->rfd[1]; // child
	}
	char *msg;
	va_list args;
	va_start(args,fmt);
	vasprintf(&msg,fmt,args);
	int nbytes = r_write(fd,msg,(size_t)strlen(msg));
	nbytes += r_write(fd,"\n",1);
	client_printf(client,"Wrote '%s'\n",msg);
	va_end(args);
	free(msg);
	return nbytes;
}

/**
 * Read a line from the child, if used in the child process
 * it reads a line from the parent.
 */
char *client_readline(Client *client) {
	int fd = client->rfd[0]; // parent
	if (client->pid==0) {
		fd=client->wfd[0]; // child
	}
	char *buf;
	r_readline(fd,buf,256);
	client_printf(client,"Read '%s'\n",buf);
	return buf;
}

/**
 * Client process function
 */
int client_main(Client *client) {
	char *resp;
	size_t buf = 256;
	client_printf(client,"Hello from the client!\n");

	while(TRUE) {
		client_printf(client,"Waiting for cmd...\n");
		char *cmd = client_readline(client);

		// TODO: Handle the cmd;
		if (h_str_in(cmd,(char *[]){"AMT\n","bet\n"})) {
			client_printf(client,"How much money are you playing with?\n");
			getline(&resp, &buf, stdin);
			client_sendline(client,"%s",resp);
		} else if (h_str_in(cmd,(char *[]){"BET\n","bet\n"}))  {
			client_printf(client,"What is your bet?\n");
			getline(&resp, &buf, stdin);
			client_sendline(client,"%s",resp);
		}/* else if (h_str_in(cmd,(char*[]){"HIT\n","hit\n"})) {
			client_printf(client,"Would you like to hit (Y/N)?\n");
			getline(&resp, &buf, stdin);
			client_sendline(client,"%s",resp);
		}
		*/
		// if cmd==bet;
		// elif cmd==hit;
		// elif cmd==cards;
		// etc...
		free(cmd);
	};
	client_printf(client,"Goodbye!\n");
	return EXIT_SUCCESS;
}

int client_printf(Client *client,const char *fmt,...) {
	int bytes = 0;
	if(client->pid==0){
		bytes +=printf("[Player%i] ",client->id);
	} else {
		bytes +=printf("[Dealer ] ");
	}
	va_list args;
	va_start(args,fmt);
	bytes +=vprintf(fmt,args);
	va_end(args);
	fflush(stdout);
	return bytes;
}

// ========================================= CLIENT =================================================

int main(int argc, char *argv[]) {
	Player *dealer;Player *player;
	Blackjack *game;
	Client *clients[14];
	int num_players;
	int i;
	char *cmd=NULL;
	size_t buf=256;

	if (argc!=2) {
		printf("Usage: blackjack <number_of_players>");
		exit(1);
	}
	num_players = atoi(argv[1]);
	if(!(1<num_players || num_players<13)) {
		printf("Usage: blackjack <number_of_players>");
		printf("Error: Can only play with 1-13 players, given %i",num_players);
		exit(1);
	}

	printf("\nWelcome to blackjack:\n");
	printf("-----------------------------------\n");

	// TODO: Create a client process for each player here
	// For now do everything in one until it's working
	for(i=1;i<num_players+1;i++) { // so index is same as player
		clients[i] = client_create(i);
	}

	// Init the the game
	num_players++; // +1 for dealer
	game = blackjack_create(num_players);
	dealer = game->players[0];


	// When a player enters the game, they will tell the dealer the
	// amount of money they will use to begin the game.
	for (i=1;i<(game->_num_players);i++) {
		player = game->players[i];

		// TODO: Ask how much they're playing with
		// TODO: Ask each player to bet and handle responses/
		//printf("Player %i, how much money are you playing with?\n",player->id);
		//getline(&cmd, &buf, stdin);
		client_sendline(clients[i],"AMT");
		cmd = client_readline(clients[i]);
		double money = strtod(cmd,NULL);

		player->money = money;
		printf("Player %i playing with $%0.2f\n",player->id,player->money);
	}

	// game loop
	while(game->_num_players>1) {
		printf("\nStarting new round:\n");
		printf("-----------------------------------\n");

		// Start the game
		assert(blackjack_init_round(game));

		printf("\nBets in:\n");
		printf("-----------------------------------\n");
		for (i=1;i<(game->_num_players);i++) {
			player = game->players[i];

			// TODO: Ask each player to bet and handle responses
			//printf("Player %i, what is your bet?\n",player->id);
			//getline(&cmd, &buf, stdin);
			client_sendline(clients[i],"BET");
			cmd = client_readline(clients[i]);
			double bet = strtod(cmd,NULL);

			if (bet>0 && player_bet(player,bet)) {
				printf("Player %i bet $%0.2f.\n",player->id,player->bet);
			} else {
				printf("Player %i left the table with $%0.2f.\n",player->id,player->money);
				assert(blackjack_remove_player(game,player));
				i--;
			}
		}

		// Nobody is playing
		if (game->_num_players<2) {
			break;
		}

		// TODO: Send player updated Player object to client so they can see their cards

		// TODO: Ask each player to hit or stay and handle responses
		for (i=1;i<(game->_num_players);i++) {
			player = game->players[i];
			printf("\nPlayer %i's turn:\n",player->id);
			printf("-----------------------------------\n");
			printf("Player %i has cards %s.\n",player->id,player_cards_to_str(player));

			client_sendline(clients[i],"CARDS:%s",player_cards_to_str(player));
			//cmd = client_readline(clients[i]);

			while (!(player->busted)) {
				//printf("Player %i, would you like to hit?\n",player->id);
				//getline(&cmd, &buf, stdin);
				client_sendline(clients[i],"HIT");
				cmd = client_readline(clients[i]);
				if (!(h_str_in(cmd,(char *[]){"Y\n","y\n"}))){break;}
				assert(blackjack_deal_card(game,player));
				printf("Player %i hit and got [%s] giving score of %i.\n",player->id,card_to_str(player->cards[player->_num_cards-1]),player->score);
				client_sendline(clients[i],"CARDS:%s\n",player_cards_to_str(player));
			}
			if (player->busted) {
				printf("Player %i busted.\n",player->id);
			} else {
				printf("Player %i stayed with score %i.\n",player->id,player->score);
			}
			//printf("%s\n\n",player_to_str(player));
		}
		//     while player_hit

		// DONE: Dealer choose to hit or stay
		// Dealer:
		//  - Always stays on 17 or higher
		//  - Always hits on 16 or lower
		printf("\nDealers turn:\n");
		printf("-----------------------------------\n");
		printf("Dealer has cards %s\n",player_cards_to_str(dealer));
		while (!(dealer->busted) && (dealer->score<17)) {
			assert(blackjack_deal_card(game,dealer));
			printf("Dealer hit and got [%s] giving score of %i\n",card_to_str(dealer->cards[dealer->_num_cards-1]),dealer->score);
		}
		if (dealer->busted) {
			printf("Dealer busted\n");
		} else {
			printf("Dealer stayed with score %i\n",dealer->score);
		}

		// DONE: Round is over print out the hands
		// TODO: Determine who won/lost and send that to the clients
		printf("\n\nRound results:\n");
		printf("-----------------------------------\n");
		for (i=1;i<(game->_num_players);i++) {
			player = game->players[i];
			int j=player_cmp(player,dealer);
			if (j>0 || player->score==21) { // won
				double won = player->bet;
				if (player->score==21) { // blackjack
					won = player->bet*1.5;
				}
				player->money+=player->bet+won;
				printf("Player %i won $%0.2f and has $%0.2f!\n",player->id,won,player->money);
			} else if(j==0) { // tied, player get's money back
				player->money+=player->bet;
				printf("Player %i tied dealer and has $%0.2f!\n",player->id,player->money);
			} else { // lost, bet already taken out when game started
				printf("Player %i lost $%0.2f and has $%0.2f!\n",player->id,player->bet,player->money);
			}
		}

		// Remove anyone that's broke
		for (i=1;i<(game->_num_players);i++) {
			player = game->players[i];
			if (player->money<5) {
				printf("Player %i left the table.\n",player->id);
				assert(blackjack_remove_player(game,player));
				i--;
			}
		}

	}



	printf("Thanks for playing! HAVE A NICE DAY!");
	return EXIT_SUCCESS;
}
