#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "log.h"

bool is_attacked_by(const struct game *game, struct square square, enum piece color);
bool is_attacked(const struct game *game, struct square square);
bool is_checked(const struct game *game, enum piece color);

const char *move_result_text[] = {
    "default",
    "check",
    "checkmate",
    "draw",
    "illegal",
};

// Starting position
const struct game setup = {
    .board = {
        { WHITE|ROOK,   WHITE|PAWN, 0, 0, 0, 0, BLACK|PAWN, BLACK|ROOK   },
        { WHITE|KNIGHT, WHITE|PAWN, 0, 0, 0, 0, BLACK|PAWN, BLACK|KNIGHT },
        { WHITE|BISHOP, WHITE|PAWN, 0, 0, 0, 0, BLACK|PAWN, BLACK|BISHOP },
        { WHITE|QUEEN,  WHITE|PAWN, 0, 0, 0, 0, BLACK|PAWN, BLACK|QUEEN  },
        { WHITE|KING,   WHITE|PAWN, 0, 0, 0, 0, BLACK|PAWN, BLACK|KING   },
        { WHITE|BISHOP, WHITE|PAWN, 0, 0, 0, 0, BLACK|PAWN, BLACK|BISHOP },
        { WHITE|KNIGHT, WHITE|PAWN, 0, 0, 0, 0, BLACK|PAWN, BLACK|KNIGHT },
        { WHITE|ROOK,   WHITE|PAWN, 0, 0, 0, 0, BLACK|PAWN, BLACK|ROOK   } },
    
    .side_to_move = WHITE,
    .white_castling_avail = KING | QUEEN,
    .black_castling_avail = KING | QUEEN,
    .en_passant_file = -1,
    .halfmove_clock = 0,
};

// Convert Forsyth-Edwards notation (FEN) to game
// Returns *game that must be freed manually
// Return NULL on incorrect FEN format
struct game* fen_to_game(char *fen)
{
    struct game* result = malloc(sizeof(struct game));
    memset(result, 0, sizeof(struct game));
    int file = 0, rank = 7;
    int i = -1;

    while (fen[++i] != ' ') {
        if (fen[i] == '/') {
            rank--;
            file = 0;
            continue;
        }

        if (file > 7 || rank < 0)
            goto ERROR;

        if (fen[i] >= '1' && fen[i] <= '8') {
            file += fen[i] - '0';
            continue;
        }

        switch (fen[i]) {
        case 'P': result->board[file][rank] = WHITE|PAWN;   break;
        case 'N': result->board[file][rank] = WHITE|KNIGHT; break;
        case 'B': result->board[file][rank] = WHITE|BISHOP; break;
        case 'R': result->board[file][rank] = WHITE|ROOK;   break;
        case 'Q': result->board[file][rank] = WHITE|QUEEN;  break;
        case 'K': result->board[file][rank] = WHITE|KING;   break;
        case 'p': result->board[file][rank] = BLACK|PAWN;   break;
        case 'n': result->board[file][rank] = BLACK|KNIGHT; break;
        case 'b': result->board[file][rank] = BLACK|BISHOP; break;
        case 'r': result->board[file][rank] = BLACK|ROOK;   break;
        case 'q': result->board[file][rank] = BLACK|QUEEN;  break;
        case 'k': result->board[file][rank] = BLACK|KING;   break;
        default: goto ERROR;
        }

        file++;
    }

    switch (fen[++i]) {
    case 'w': result->side_to_move = WHITE; break;
    case 'b': result->side_to_move = BLACK; break;
    default: goto ERROR;
    }

    if (fen[++i] != ' ')
        goto ERROR;

    while (fen[++i] != ' ') {
        switch (fen[i]) {
        case '-': break;
        case 'K': result->white_castling_avail |= KING;  break;
        case 'Q': result->white_castling_avail |= QUEEN; break;
        case 'k': result->black_castling_avail |= KING;  break;
        case 'q': result->black_castling_avail |= QUEEN; break;
        default: goto ERROR;
        }
    }

    i++;
    if (fen[i] >= 'a' && fen[i] <= 'h') {
        result->en_passant_file = fen[i] - 'a';
        if (result->en_passant_file < 0 || result->en_passant_file > 7)
            goto ERROR;
        i++;
        if (fen[i] < '1' || fen[i] > '8')
            goto ERROR;
    } else {
        if (fen[i] != '-')
            goto ERROR;
    }

    i += 2;
    if (sscanf(fen + i, "%d", &(result->halfmove_clock)) != 1)
        goto ERROR;
    if (result->halfmove_clock < 0 || result->halfmove_clock > 100)
        goto ERROR;

    return result; 

ERROR:
    free(result);
    return NULL;
}

enum piece piece_at(const struct game *game, struct square square)
{
    return game->board[square.file][square.rank]; 
} 

/*
 * Get the game hash, the Zobrist algorithm
 * The hash of a same game may differ across the program runs.
 */
int hash(const struct game *game)
{
    static bool init = false;
    static int piece_hash[8][8][12]; // random nubers for each square-piece
    static int en_passant_hash[8];
    static int castling_avail_hash[4];
    static int white_to_move_hash;
    if (!init) {
        for (int file = 0; file < 8; file++)
        for (int rank = 0; rank < 8; rank++)
        for (int piece = 0; piece < 12; piece++)
            piece_hash[file][rank][piece] = rand();
        for (int file = 0; file < 8; file++)
            en_passant_hash[file] = rand();
        for (int i = 0; i < 4; i++)
            castling_avail_hash[i] = rand();
        white_to_move_hash = rand();
        init = true;
    }

    int result = 0;
    struct square square;
    for (square.file = 0; square.file < 8; square.file++)
    for (square.rank = 0; square.rank < 8; square.rank++) {
        switch(piece_at(game, square)) {
        case WHITE|PAWN:   result ^= piece_hash[square.file][square.rank][0];  break;
        case WHITE|KNIGHT: result ^= piece_hash[square.file][square.rank][1];  break;
        case WHITE|BISHOP: result ^= piece_hash[square.file][square.rank][2];  break;
        case WHITE|ROOK:   result ^= piece_hash[square.file][square.rank][3];  break;
        case WHITE|QUEEN:  result ^= piece_hash[square.file][square.rank][4];  break;
        case WHITE|KING:   result ^= piece_hash[square.file][square.rank][5];  break;
        case BLACK|PAWN:   result ^= piece_hash[square.file][square.rank][6];  break;
        case BLACK|KNIGHT: result ^= piece_hash[square.file][square.rank][7];  break;
        case BLACK|BISHOP: result ^= piece_hash[square.file][square.rank][8];  break;
        case BLACK|ROOK:   result ^= piece_hash[square.file][square.rank][9];  break;
        case BLACK|QUEEN:  result ^= piece_hash[square.file][square.rank][10]; break;
        case BLACK|KING:   result ^= piece_hash[square.file][square.rank][11]; break;
        }
    } 

    // the position is different if a pawn can no longer be taken en passant
    if (game->en_passant_file >= 0) {
        enum piece moving_pawn = game->side_to_move | PAWN;
        struct square en_passant_pawn;
        en_passant_pawn.rank = game->side_to_move == WHITE ? 4 : 3;
        en_passant_pawn.file = game->en_passant_file - 1;
        if (en_passant_pawn.file >= 1 && piece_at(game, en_passant_pawn) == moving_pawn) {
            result ^= en_passant_hash[game->en_passant_file];
        } else {
            en_passant_pawn.file = game->en_passant_file + 1;
            if (en_passant_pawn.file <= 6 && piece_at(game, en_passant_pawn) == moving_pawn) {
                result ^= en_passant_hash[game->en_passant_file];
            }
        }
    }

    // castling availability is accounted even if the king cannot castle at the moment
    if (game->white_castling_avail & QUEEN)
        result ^= castling_avail_hash[0];
    if (game->white_castling_avail & KING)
        result ^= castling_avail_hash[1];
    if (game->black_castling_avail & QUEEN)
        result ^= castling_avail_hash[2];
    if (game->black_castling_avail & KING)
        result ^= castling_avail_hash[3];
    if (game->side_to_move == WHITE)
        result ^= white_to_move_hash;

    return result;
}

/*
 * Check the destination correctness and the free way to it.
 * We already know that there is no own piece in the destination.
 */
bool pawn_has_way(const struct game *game, struct square from, struct square to)
{
    assert((piece_at(game, from) & PIECE_TYPE) == PAWN && "checking not pawn");
    int direction = ((piece_at(game, from) & COLOR) == WHITE) ? 1 : -1;
    int advance = to.rank - from.rank;

    // Just move, no capture
    if (from.file == to.file) {
        if (piece_at(game, to) != EMPTY)
            return false;
        if (advance == direction && piece_at(game, to) == EMPTY)
            return true;
        if (advance == 2 * direction) {
            int pawn_start_rank = (game->side_to_move == WHITE) ? 1 : 6;
            if (from.rank != pawn_start_rank)
                return false;
            if (game->board[from.file][pawn_start_rank + direction] != EMPTY)
                return false;
            return true;
        }
        return false;
    }

    // Capture
    if (abs(from.file - to.file) != 1)
        return false;
    if (advance != direction)
        return false;
    if (piece_at(game, to) != EMPTY)
        return true;
    int en_passant_rank = (game->side_to_move == WHITE) ? 5 : 2;
    if (to.file == game->en_passant_file && to.rank == en_passant_rank)
        return true;
    return false;
}

bool knight_has_way(struct square from, struct square to)
{
    int file_move = abs(from.file - to.file);
    int rank_move = abs(from.rank - to.rank);
    if ((file_move == 1 && rank_move == 2) || (file_move == 2 && rank_move == 1))
        return true;
    return false; 
}

bool bishop_has_way(const struct game *game, struct square from, struct square to)
{
    int file_move = abs(from.file - to.file);
    int rank_move = abs(from.rank - to.rank);
    if (file_move != rank_move)
        return false;
    int file_direction = (to.file > from.file) ? 1 : -1;
    int rank_direction = (to.rank > from.rank) ? 1 : -1;
    int file = from.file;
    int rank = from.rank;
    for (int i = 1; i < file_move; i++) {
        file += file_direction;
        rank += rank_direction;
        if (game->board[file][rank] != EMPTY)
            return false;
    }
    return true;
}

bool rook_has_way(const struct game *game, struct square from, struct square to)
{
    if (from.file != to.file && from.rank != to.rank)
        return false;

    if (from.file == to.file) {
        int direction = (to.rank > from.rank) ? 1 : -1;
        for (int rank = from.rank + direction; rank != to.rank; rank += direction)
            if (game->board[from.file][rank] != EMPTY)
                return false;
    }

    if (from.rank == to.rank) {
        int direction = (to.file > from.file) ? 1 : -1;
        for (int file = from.file + direction; file != to.file; file += direction)
            if (game->board[file][from.rank] != EMPTY)
                return false;
    } 

    return true;
}

bool queen_has_way(const struct game *game, struct square from, struct square to)
{
    assert((piece_at(game, from) & PIECE_TYPE) == QUEEN && "checking not queen");
    return bishop_has_way(game, from, to) || rook_has_way(game, from, to);
}

bool king_has_way(const struct game *game, struct square from, struct square to)
{
    assert((piece_at(game, from) & PIECE_TYPE) == KING && "checking not king");

    int file_move = abs(from.file - to.file);
    int rank_move = abs(from.rank - to.rank);

    // castling
    if (file_move == 2 && rank_move == 0) {
        // didn't the king and the rook move?
        enum piece castling_side = (to.file > from.file) ? KING : QUEEN;
        enum piece color = piece_at(game, from) & COLOR;
        if (color == WHITE) {
            if (!(castling_side & game->white_castling_avail))
                return false;
        } else {
            if (!(castling_side & game->black_castling_avail))
                return false;
        }
        // free squares
        int direction = (castling_side == QUEEN) ? -1 : 1;
        struct square rook = {(castling_side == QUEEN) ? 0 : 7, from.rank};
        struct square rook_to = {(castling_side == QUEEN) ? 3 : 5, from.rank};
        enum piece opp_color = (color == WHITE) ? BLACK : WHITE;
        for (int file = from.file + direction; file != rook.file; file += direction)
            if (game->board[file][from.rank] != EMPTY)
                return false;
        // cannot castle when old or intermediate king position is checked
        if (is_attacked(game, from) || is_attacked_by(game, rook_to, opp_color))
            return false;
        return true;
    }

    if (file_move > 1 || rank_move > 1)
        return false;

    // A move into check will be checked later

    return true;
}

bool piece_has_way(const struct game *game, struct square from, struct square to)
{
    switch(piece_at(game, from) & PIECE_TYPE) {
    case PAWN:
        return pawn_has_way(game, from, to);

    case KNIGHT:
        return knight_has_way(from, to);

    case BISHOP:
        return bishop_has_way(game, from, to);

    case ROOK:
        return rook_has_way(game, from, to);

    case QUEEN:
        return queen_has_way(game, from, to);

    case KING:
        return king_has_way(game, from, to);
    }
    assert(false && "piece_has_way()");
    return false;
}

bool is_attacked_by(const struct game *game, struct square square, enum piece color)
{
    struct square from;
    for (from.file = 0; from.file < 8; from.file++) {
        for (from.rank = 0; from.rank < 8; from.rank++) {
            enum piece piece = piece_at(game, from);
            if ((piece & color) && (piece_has_way(game, from, square))) {
                log_debug("%c%d is attacked by %c%d",
                    'a' + square.file, 1 + square.rank, 'a' + from.file, 1 + from.rank);
                return true;
            }
        }
    }
    return false;
}

bool is_attacked(const struct game *game, struct square square)
{
    assert(piece_at(game, square) != EMPTY && "is_attacked() empty square");
    enum piece opp_color = ((piece_at(game, square) & COLOR) == WHITE) ? BLACK : WHITE;
    return is_attacked_by(game, square, opp_color);
}

bool is_checked(const struct game *game, enum piece color)
{
    struct square king;
    for (king.file = 0; king.file < 8; king.file++)
        for (king.rank = 0; king.rank < 8; king.rank++)
            if (game->board[king.file][king.rank] == (KING | color)) {
                log_debug("king found at %c%d", 'a' + king.file, 0 + king.rank);
                return is_attacked(game, king);
            }
    assert(false && "king not found");
    return false;
}

/*
 * Generic movement restrictions
 */

bool is_legal_move(const struct game *game, struct square from,
                   struct square to, enum piece promotion)
{
    if (from.rank < 0 || from.rank > 7 ||
        from.file < 0 || from.file > 7 ||
        to.rank   < 0 || to.rank   > 7 ||
        to.file   < 0 || to.file   > 7)
    {
        log_warning("Can't move out of the board");
        return false;
    }
    
    if (piece_at(game, from) == EMPTY) {
        //log_warning("Must move a piece");
        return false;
    }

    if ((piece_at(game, from) & COLOR) != game->side_to_move) {
        //log_warning("Must move own piece");
        return false;
    }

    if ((piece_at(game, to) & COLOR) == game->side_to_move) {
        //log_warning("Can't capture own piece");
        return false;
    }

    if (!piece_has_way(game, from, to)) {
        //log_warning("No way from %c%d to %c%d",
        //    'a' + from.file, 1 + from.rank, 'a' + to.file, 1 + to.rank);
        return false;
    }

    int last_rank = (game->side_to_move == WHITE) ? 7 : 0;
    if ((piece_at(game, from) & PAWN) && (to.rank == last_rank)) {
        switch (promotion & PIECE_TYPE) {
        case KNIGHT:
        case BISHOP:
        case ROOK:
        case QUEEN:
            break;
        default:
            //log_warning("Promotion not specified");
            return false;
        }
    }
    else
        if (promotion != EMPTY) {
            //log_warning("Can't promote");
            return false;
        }

    // Isn't own king checked?
    struct game new_position = *game;
    new_position.board[to.file][to.rank] = piece_at(game, from);
    new_position.board[from.file][from.rank] = EMPTY;
    new_position.side_to_move = (game->side_to_move == WHITE) ? BLACK : WHITE;
    new_position.en_passant_file = -1;
    if (is_checked(&new_position, game->side_to_move)) {
        log_debug("Can't move into check");
        return false;
    }

    return true;
}

bool can_make_any_move(const struct game *game)
{
    // Not optimal, but neither is performance-critical
    struct square from;
    struct square to;
    for (from.file = 0; from.file < 8; from.file++)
    for (from.rank = 0; from.rank < 8; from.rank++)
        if (piece_at(game, from) & game->side_to_move)
            for (to.file = 0; to.file < 8; to.file++)
            for (to.rank = 0; to.rank < 8; to.rank++) {
                enum piece promotion = EMPTY;
                if ((piece_at(game, from) & PAWN) && (to.rank == 0 || to.rank == 7))
                    promotion = QUEEN;
                if (is_legal_move(game, from, to, promotion))
                    return true;
            }
    return false;
}

bool enough_material(struct game *game)
{
    int w_knights = 0, w_bishops = 0;
    int b_knights = 0, b_bishops = 0;
    struct square square;
    for (square.file = 0; square.file < 8; square.file++)
    for (square.rank = 0; square.rank < 8; square.rank++) {
        switch (piece_at(game, square)) {
        case WHITE|PAWN:
        case WHITE|ROOK:
        case WHITE|QUEEN:
        case BLACK|ROOK:
        case BLACK|PAWN:
        case BLACK|QUEEN:
            return true;
        case WHITE|KNIGHT:
            w_knights++;
            break;
        case BLACK|KNIGHT:
            b_knights++;
            break;
        case WHITE|BISHOP:
            w_bishops++;
            break;
        case BLACK|BISHOP:
            b_bishops++;
            break;
        }
    }
    if (w_bishops >= 2 || b_bishops >= 2)
        return true;
    if ((w_bishops == 1 && w_knights >= 1) || (b_bishops == 1 && b_knights >= 1))
        return true;
    return false;
}

/*
 * Make a move, modifying the input game structure (if the move is legal) and
 * returning the result (default, check, checkmate, draw, or illegal move).
 */
enum move_result move(struct game *game, struct square from, struct square to,
                      enum piece promotion)
{
    if (!is_legal_move(game, from, to, promotion))
        return ILLEGAL;

    // game setup position
    if (game->halfmove_clock == 0)
        game->position_history[0] = hash(game);

    // disabling castling
    if (from.file == 0 && from.rank == 0)
        game->white_castling_avail &= ~QUEEN;
    if (from.file == 4 && from.rank == 0)
        game->white_castling_avail = EMPTY;
    if (from.file == 7 && from.rank == 0)
        game->white_castling_avail &= ~KING;
    if (from.file == 0 && from.rank == 7)
        game->black_castling_avail &= ~QUEEN;
    if (from.file == 4 && from.rank == 7)
        game->black_castling_avail = EMPTY;
    if (from.file == 7 && from.rank == 7)
        game->black_castling_avail &= ~KING;

    // moving the rook for castling
    if ((piece_at(game, from) & KING) && (to.file - from.file == 2)) {
        game->board[5][from.rank] = game->board[7][from.rank];
        game->board[7][from.rank] = EMPTY;
    }
    if ((piece_at(game, from) & KING) && (to.file - from.file == -2)) {
        game->board[3][from.rank] = game->board[0][from.rank];
        game->board[0][from.rank] = EMPTY;
    }

    // en passant availability
    game->en_passant_file = -1;
    if ((piece_at(game, from) & PAWN) && abs(from.rank - to.rank) == 2) {
        log_debug("Available en passant at file %c", 'a' + from.file);
        game->en_passant_file = from.file;
    }

    // track the fifty-move rule
    game->halfmove_clock++;
    if (piece_at(game, from) & PAWN || piece_at(game, to) != EMPTY)
        game->halfmove_clock = 0;

    // move the piece
    game->board[to.file][to.rank] = game->board[from.file][from.rank];
    game->board[from.file][from.rank] = EMPTY;
    if (promotion != EMPTY)
        game->board[to.file][to.rank] = ((promotion & ~COLOR) | game->side_to_move);
    game->side_to_move = (game->side_to_move == WHITE) ? BLACK : WHITE;

    // remove a pawn taken en passant
    if ((piece_at(game, from) & PAWN) && (from.file != to.file) &&
            (piece_at(game, to) == EMPTY)) {
        game->board[to.file][from.rank] == EMPTY;
    }

    game->position_history[game->halfmove_clock] = hash(game);
    int repetitions = 0;
    for (int move = 0; move <= game->halfmove_clock; move++)
        if (game->position_history[move] == game->position_history[game->halfmove_clock])
            repetitions++;
    if (repetitions == 3)
        return DRAW;

    if (!enough_material(game))
        return DRAW;
    if (!can_make_any_move(game)) {
        if (is_checked(game, game->side_to_move))
            return CHECKMATE;
        else
            return DRAW;
    }
    if (game->halfmove_clock == 100)
        return DRAW;
    if (is_checked(game, game->side_to_move))
        return CHECK;
        
    return DEFAULT;
} 

enum move_result parse_move(struct game *game, char *move_str)
{
    // strip newline characters
    int length = strcspn(move_str, "\r\n");
    move_str[length] = '\0';
    if (length < 4 || length > 5) {
        log_warning("Incorrect move '%s'", move_str);
        return ILLEGAL;
    }
    for (int i = 0; move_str[i]; i++)  // lower case
        move_str[i] = tolower(move_str[i]);

    struct square from, to;
    enum piece promotion = EMPTY;
    from.file = move_str[0] - 'a';
    from.rank = move_str[1] - '1';
    to.file = move_str[2] - 'a'; 
    to.rank = move_str[3] - '1';
    switch (move_str[4]) {
    case 'n': promotion = KNIGHT; break;
    case 'b': promotion = BISHOP; break;
    case 'r': promotion = ROOK; break;
    case 'q': promotion = QUEEN; break;
    }

    return move(game, from, to, promotion);
}
