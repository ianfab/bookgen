/*
  Fairy-Stockfish, a UCI chess variant playing engine derived from Stockfish
  Copyright (C) 2018-2021 Fabian Fichter

  Fairy-Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Fairy-Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <vector>
#include <string>
#include <sstream>
#include <cctype>
#include <iostream>
#include <math.h>

#include "types.h"
#include "variant.h"


namespace PSQT {
void init(const Variant* v);
}

enum Notation {
    NOTATION_DEFAULT,
    // https://en.wikipedia.org/wiki/Algebraic_notation_(chess)
    NOTATION_SAN,
    NOTATION_LAN,
    // https://en.wikipedia.org/wiki/Shogi_notation#Western_notation
    NOTATION_SHOGI_HOSKING, // Examples: P76, S’34
    NOTATION_SHOGI_HODGES, // Examples: P-7f, S*3d
    NOTATION_SHOGI_HODGES_NUMBER, // Examples: P-76, S*34
    // http://www.janggi.pl/janggi-notation/
    NOTATION_JANGGI,
    // https://en.wikipedia.org/wiki/Xiangqi#Notation
    NOTATION_XIANGQI_WXF,
};

Notation default_notation(const Variant* v) {
    if (v->variantTemplate == "shogi")
        return NOTATION_SHOGI_HODGES_NUMBER;
    return NOTATION_SAN;
}

enum Disambiguation {
    NO_DISAMBIGUATION,
    FILE_DISAMBIGUATION,
    RANK_DISAMBIGUATION,
    SQUARE_DISAMBIGUATION,
};

bool is_shogi(Notation n) {
    return n == NOTATION_SHOGI_HOSKING || n == NOTATION_SHOGI_HODGES || n == NOTATION_SHOGI_HODGES_NUMBER;
}

std::string piece(const Position& pos, Move m, Notation n) {
    Color us = pos.side_to_move();
    Square from = from_sq(m);
    Piece pc = pos.moved_piece(m);
    PieceType pt = type_of(pc);
    // Quiet pawn moves
    if ((n == NOTATION_SAN || n == NOTATION_LAN) && type_of(pc) == PAWN && type_of(m) != DROP)
        return "";
    // Tandem pawns
    else if (n == NOTATION_XIANGQI_WXF && popcount(pos.pieces(us, pt) & file_bb(from)) > 2)
        return std::to_string(popcount(forward_file_bb(us, from) & pos.pieces(us, pt)) + 1);
    // Moves of promoted pieces
    else if (is_shogi(n) && type_of(m) != DROP && pos.unpromoted_piece_on(from))
        return "+" + std::string(1, toupper(pos.piece_to_char()[pos.unpromoted_piece_on(from)]));
    // Promoted drops
    else if (is_shogi(n) && type_of(m) == DROP && dropped_piece_type(m) != in_hand_piece_type(m))
        return "+" + std::string(1, toupper(pos.piece_to_char()[in_hand_piece_type(m)]));
    else if (pos.piece_to_char_synonyms()[pc] != ' ')
        return std::string(1, toupper(pos.piece_to_char_synonyms()[pc]));
    else
        return std::string(1, toupper(pos.piece_to_char()[pc]));
}

std::string file(const Position& pos, Square s, Notation n) {
    switch (n) {
    case NOTATION_SHOGI_HOSKING:
    case NOTATION_SHOGI_HODGES:
    case NOTATION_SHOGI_HODGES_NUMBER:
        return std::to_string(pos.max_file() - file_of(s) + 1);
    case NOTATION_JANGGI:
        return std::to_string(file_of(s) + 1);
    case NOTATION_XIANGQI_WXF:
        return std::to_string((pos.side_to_move() == WHITE ? pos.max_file() - file_of(s) : file_of(s)) + 1);
    default:
        return std::string(1, char('a' + file_of(s)));
    }
}

std::string rank(const Position& pos, Square s, Notation n) {
    switch (n) {
    case NOTATION_SHOGI_HOSKING:
    case NOTATION_SHOGI_HODGES_NUMBER:
        return std::to_string(pos.max_rank() - rank_of(s) + 1);
    case NOTATION_SHOGI_HODGES:
        return std::string(1, char('a' + pos.max_rank() - rank_of(s)));
    case NOTATION_JANGGI:
        return std::to_string((pos.max_rank() - rank_of(s) + 1) % 10);
    case NOTATION_XIANGQI_WXF:
    {
        if (pos.empty(s))
            return std::to_string(relative_rank(pos.side_to_move(), s, pos.max_rank()) + 1);
        else if (pos.pieces(pos.side_to_move(), type_of(pos.piece_on(s))) & forward_file_bb(pos.side_to_move(), s))
            return "-";
        else
            return "+";
    }
    default:
        return std::to_string(rank_of(s) + 1);
    }
}

std::string square(const Position& pos, Square s, Notation n) {
    switch (n) {
    case NOTATION_JANGGI:
        return rank(pos, s, n) + file(pos, s, n);
    default:
        return file(pos, s, n) + rank(pos, s, n);
    }
}

Disambiguation disambiguation_level(const Position& pos, Move m, Notation n) {
    // Drops never need disambiguation
    if (type_of(m) == DROP)
        return NO_DISAMBIGUATION;

    // NOTATION_LAN and Janggi always use disambiguation
    if (n == NOTATION_LAN || n == NOTATION_JANGGI)
        return SQUARE_DISAMBIGUATION;

    Color us = pos.side_to_move();
    Square from = from_sq(m);
    Square to = to_sq(m);
    Piece pc = pos.moved_piece(m);
    PieceType pt = type_of(pc);

    // Xiangqi uses either file disambiguation or +/- if two pieces on file
    if (n == NOTATION_XIANGQI_WXF)
    {
        // Disambiguate by rank (+/-) if target square of other piece is valid
        if (popcount(pos.pieces(us, pt) & file_bb(from)) == 2)
        {
            Square otherFrom = lsb((pos.pieces(us, pt) & file_bb(from)) ^ from);
            Square otherTo = otherFrom + Direction(to) - Direction(from);
            if (is_ok(otherTo) && (pos.board_bb(us, pt) & otherTo))
                return RANK_DISAMBIGUATION;
        }
        return FILE_DISAMBIGUATION;
    }

    // Pawn captures always use disambiguation
    if (n == NOTATION_SAN && pt == PAWN)
    {
        if (pos.capture(m))
            return FILE_DISAMBIGUATION;
        if (type_of(m) == PROMOTION && from != to && pos.sittuyin_promotion())
            return SQUARE_DISAMBIGUATION;
    }

    // A disambiguation occurs if we have more then one piece of type 'pt'
    // that can reach 'to' with a legal move.
    Bitboard b = pos.pieces(us, pt) ^ from;
    Bitboard others = 0;

    while (b)
    {
        Square s = pop_lsb(&b);
        if (   pos.pseudo_legal(make_move(s, to))
               && pos.legal(make_move(s, to))
               && !(is_shogi(n) && pos.unpromoted_piece_on(s) != pos.unpromoted_piece_on(from)))
            others |= s;
    }

    if (!others)
        return NO_DISAMBIGUATION;
    else if (is_shogi(n))
        return SQUARE_DISAMBIGUATION;
    else if (!(others & file_bb(from)))
        return FILE_DISAMBIGUATION;
    else if (!(others & rank_bb(from)))
        return RANK_DISAMBIGUATION;
    else
        return SQUARE_DISAMBIGUATION;
}

std::string disambiguation(const Position& pos, Square s, Notation n, Disambiguation d) {
    switch (d)
    {
    case FILE_DISAMBIGUATION:
        return file(pos, s, n);
    case RANK_DISAMBIGUATION:
        return rank(pos, s, n);
    case SQUARE_DISAMBIGUATION:
        return square(pos, s, n);
    default:
        assert(d == NO_DISAMBIGUATION);
        return "";
    }
}

const std::string move_to_san(Position& pos, Move m, Notation n) {
    std::string san = "";
    Color us = pos.side_to_move();
    Square from = from_sq(m);
    Square to = to_sq(m);

    if (type_of(m) == CASTLING)
    {
        san = to > from ? "O-O" : "O-O-O";

        if (is_gating(m))
        {
            san += std::string("/") + pos.piece_to_char()[make_piece(WHITE, gating_type(m))];
            san += square(pos, gating_square(m), n);
        }
    }
    else
    {
        // Piece
        san += piece(pos, m, n);

        // Origin square, disambiguation
        Disambiguation d = disambiguation_level(pos, m, n);
        san += disambiguation(pos, from, n, d);

        // Separator/Operator
        if (type_of(m) == DROP)
            san += n == NOTATION_SHOGI_HOSKING ? '\'' : is_shogi(n) ? '*' : '@';
        else if (n == NOTATION_XIANGQI_WXF)
        {
            if (rank_of(from) == rank_of(to))
                san += '=';
            else if (relative_rank(us, to, pos.max_rank()) > relative_rank(us, from, pos.max_rank()))
                san += '+';
            else
                san += '-';
        }
        else if (pos.capture(m))
            san += 'x';
        else if (n == NOTATION_LAN || (is_shogi(n) && (n != NOTATION_SHOGI_HOSKING || d == SQUARE_DISAMBIGUATION)) || n == NOTATION_JANGGI)
            san += '-';

        // Destination square
        if (n == NOTATION_XIANGQI_WXF && type_of(m) != DROP)
            san += file_of(to) == file_of(from) ? std::to_string(std::abs(rank_of(to) - rank_of(from))) : file(pos, to, n);
        else
            san += square(pos, to, n);

        // Suffix
        if (type_of(m) == PROMOTION)
            san += std::string("=") + pos.piece_to_char()[make_piece(WHITE, promotion_type(m))];
        else if (type_of(m) == PIECE_PROMOTION)
            san += is_shogi(n) ? std::string("+") : std::string("=") + pos.piece_to_char()[make_piece(WHITE, pos.promoted_piece_type(type_of(pos.moved_piece(m))))];
        else if (type_of(m) == PIECE_DEMOTION)
            san += is_shogi(n) ? std::string("-") : std::string("=") + std::string(1, pos.piece_to_char()[pos.unpromoted_piece_on(from)]);
        else if (type_of(m) == NORMAL && is_shogi(n) && pos.pseudo_legal(make<PIECE_PROMOTION>(from, to)))
            san += std::string("=");
        if (is_gating(m))
            san += std::string("/") + pos.piece_to_char()[make_piece(WHITE, gating_type(m))];
    }

    // Check and checkmate
    if (pos.gives_check(m) && !is_shogi(n))
    {
        StateInfo st;
        pos.do_move(m, st);
        san += MoveList<LEGAL>(pos).size() ? "+" : "#";
        pos.undo_move(m);
    }

    return san;
}

bool hasInsufficientMaterial(Color c, const Position& pos) {

    // Other win rules
    if (   pos.captures_to_hand()
           || pos.count_in_hand(c, ALL_PIECES)
           || pos.extinction_value() != VALUE_NONE
           || (pos.capture_the_flag_piece() && pos.count(c, pos.capture_the_flag_piece())))
        return false;

    // Restricted pieces
    Bitboard restricted = pos.pieces(~c, KING);
    for (PieceType pt : pos.piece_types())
        if (pt == KING || !(pos.board_bb(c, pt) & pos.board_bb(~c, KING)))
            restricted |= pos.pieces(c, pt);

    // Mating pieces
    for (PieceType pt : { ROOK, QUEEN, ARCHBISHOP, CHANCELLOR, SILVER, GOLD, COMMONER, CENTAUR })
        if ((pos.pieces(c, pt) & ~restricted) || (pos.count(c, PAWN) && pos.promotion_piece_types().find(pt) != pos.promotion_piece_types().end()))
            return false;

    // Color-bound pieces
    Bitboard colorbound = 0, unbound;
    for (PieceType pt : { BISHOP, FERS, FERS_ALFIL, ALFIL, ELEPHANT })
        colorbound |= pos.pieces(pt) & ~restricted;
    unbound = pos.pieces() ^ restricted ^ colorbound;
    if ((colorbound & pos.pieces(c)) && (((DarkSquares & colorbound) && (~DarkSquares & colorbound)) || unbound))
        return false;

    // Unbound pieces require one helper piece of either color
    if ((pos.pieces(c) & unbound) && (popcount(pos.pieces() ^ restricted) >= 2 || pos.stalemate_value() != VALUE_DRAW))
        return false;

    return true;
}

namespace fen {

enum FenValidation : int {
    FEN_MISSING_SPACE_DELIM = -12,
    FEN_INVALID_NB_PARTS = -11,
    FEN_INVALID_CHAR = -10,
    FEN_TOUCHING_KINGS = -9,
    FEN_INVALID_BOARD_GEOMETRY = -8,
    FEN_INVALID_POCKET_INFO = -7,
    FEN_INVALID_SIDE_TO_MOVE = -6,
    FEN_INVALID_CASTLING_INFO = -5,
    FEN_INVALID_EN_PASSANT_SQ = -4,
    FEN_INVALID_NUMBER_OF_KINGS = -3,
    FEN_INVALID_HALF_MOVE_COUNTER = -2,
    FEN_INVALID_MOVE_COUNTER = -1,
    FEN_EMPTY = 0,
    FEN_OK = 1
};
enum Validation : int {
    NOK,
    OK
};

struct CharSquare {
    int rowIdx;
    int fileIdx;
    CharSquare() : rowIdx(-1), fileIdx(-1) {}
    CharSquare(int rowIdx, int fileIdx) : rowIdx(rowIdx), fileIdx(fileIdx) {}
};

bool operator==(const CharSquare& s1, const CharSquare& s2) {
    return s1.rowIdx == s2.rowIdx && s1.fileIdx == s2.fileIdx;
}

bool operator!=(const CharSquare& s1, const CharSquare& s2) {
    return !(s1 == s2);
}

int non_root_euclidian_distance(const CharSquare& s1, const CharSquare& s2) {
    return pow(s1.rowIdx - s2.rowIdx, 2) + pow(s1.fileIdx - s2.fileIdx, 2);
}

class CharBoard {
private:
    int nbRanks;
    int nbFiles;
    std::vector<char> board;  // fill an array where the pieces are for later geometry checks
public:
    CharBoard(int nbRanks, int nbFiles) : nbRanks(nbRanks), nbFiles(nbFiles) {
        assert(nbFiles > 0 && nbRanks > 0);
        board = std::vector<char>(nbRanks * nbFiles, ' ');
    }
    void set_piece(int rankIdx, int fileIdx, char c) {
        board[rankIdx * nbFiles + fileIdx] = c;
    }
    char get_piece(int rowIdx, int fileIdx) const {
        return board[rowIdx * nbFiles + fileIdx];
    }
    int get_nb_ranks() const {
        return nbRanks;
    }
    int get_nb_files() const {
        return nbFiles;
    }
    /// Returns the square of a given character
    CharSquare get_square_for_piece(char piece) const {
        CharSquare s;
        for (int r = 0; r < nbRanks; ++r) {
            for (int c = 0; c < nbFiles; ++c) {
                if (get_piece(r, c) == piece) {
                    s.rowIdx = r;
                    s.fileIdx = c;
                    return s;
                }
            }
        }
        return s;
    }
    /// Returns all square positions for a given piece
    std::vector<CharSquare> get_squares_for_piece(char piece) const {
        std::vector<CharSquare> squares;
        for (int r = 0; r < nbRanks; ++r) {
            for (int c = 0; c < nbFiles; ++c) {
                if (get_piece(r, c) == piece) {
                    squares.emplace_back(CharSquare(r, c));
                }
            }
        }
        return squares;
    }
    /// Checks if a given character is on a given rank index
    bool is_piece_on_rank(char piece, int rowIdx) const {
        for (int f = 0; f < nbFiles; ++f)
            if (get_piece(rowIdx, f) == piece)
                return true;
        return false;
    }
    friend std::ostream& operator<<(std::ostream& os, const CharBoard& board);
};

std::ostream& operator<<(std::ostream& os, const CharBoard& board) {
    for (int r = 0; r < board.nbRanks; ++r) {
        for (int c = 0; c < board.nbFiles; ++c) {
            os << "[" << board.get_piece(r, c) << "] ";
        }
        os << std::endl;
    }
    return os;
}

Validation check_for_valid_characters(const std::string& firstFenPart, const std::string& validSpecialCharacters, const Variant* v) {
    for (char c : firstFenPart) {
        if (!isdigit(c) && v->pieceToChar.find(c) == std::string::npos && validSpecialCharacters.find(c) == std::string::npos) {
            std::cerr << "Invalid piece character: '" << c << "'." << std::endl;
            return NOK;
        }
    }
    return OK;
}

std::vector<std::string> get_fen_parts(const std::string& fullFen, char delim) {
    std::vector<std::string> fenParts;
    std::string curPart;
    std::stringstream ss(fullFen);
    while (std::getline(ss, curPart, delim))
        fenParts.emplace_back(curPart);
    return fenParts;
}

/// fills the character board according to a given FEN string
Validation fill_char_board(CharBoard& board, const std::string& fenBoard, const std::string& validSpecialCharacters, const Variant* v) {
    int rankIdx = 0;
    int fileIdx = 0;

    char prevChar = '?';
    for (char c : fenBoard) {
        if (c == ' ' || c == '[')
            break;
        if (isdigit(c)) {
            fileIdx += c - '0';
            // if we have multiple digits attached we can add multiples of 9 to compute the resulting number (e.g. -> 21 = 2 + 2 * 9 + 1)
            if (isdigit(prevChar))
                fileIdx += 9 * (prevChar - '0');
        }
        else if (c == '/') {
            ++rankIdx;
            if (fileIdx != board.get_nb_files()) {
                std::cerr << "curRankWidth != nbFiles: " << fileIdx << " != " << board.get_nb_files() << std::endl;
                return NOK;
            }
            if (rankIdx == board.get_nb_ranks())
                break;
            fileIdx = 0;
        }
        else if (validSpecialCharacters.find(c) == std::string::npos) {  // normal piece
            if (fileIdx == board.get_nb_files()) {
                std::cerr << "File index: " << fileIdx << " for piece '" << c << "' exceeds maximum of allowed number of files: " << board.get_nb_files() << "." << std::endl;
                return NOK;
            }
            board.set_piece(v->maxRank-rankIdx, fileIdx, c);  // we mirror the rank index because the black pieces are given first in the FEN
            ++fileIdx;
        }
        prevChar = c;
    }

    if (v->pieceDrops) { // pockets can either be defined by [] or /
        if (rankIdx+1 != board.get_nb_ranks() && rankIdx != board.get_nb_ranks()) {
            std::cerr << "Invalid number of ranks. Expected: " << board.get_nb_ranks() << " Actual: " << rankIdx+1 << std::endl;
            return NOK;
        }
    }
    else {
        if (rankIdx+1 != board.get_nb_ranks()) {
            std::cerr << "Invalid number of ranks. Expected: " << board.get_nb_ranks() << " Actual: " << rankIdx+1 << std::endl;
            return NOK;
        }
    }
    return OK;
}

Validation fill_castling_info_splitted(const std::string& castlingInfo, std::array<std::string, 2>& castlingInfoSplitted) {
    for (char c : castlingInfo) {
        if (c != '-') {
            if (!isalpha(c)) {
                std::cerr << "Invalid castling specification: '" << c << "'." << std::endl;
                return NOK;
            }
            else if (isupper(c))
                castlingInfoSplitted[WHITE] += tolower(c);
            else
                castlingInfoSplitted[BLACK] += c;
        }
    }
    return OK;
}

std::string color_to_string(Color c) {
    switch (c) {
    case WHITE:
        return "WHITE";
    case BLACK:
        return "BLACK";
    case COLOR_NB:
        return "COLOR_NB";
    default:
        return "INVALID_COLOR";
    }
}

Validation check_960_castling(const std::array<std::string, 2>& castlingInfoSplitted, const CharBoard& board, const std::array<CharSquare, 2>& kingPositionsStart) {

    for (Color color : {WHITE, BLACK}) {
        for (char charPiece : {'K', 'R'}) {
            if (castlingInfoSplitted[color].size() == 0)
                continue;
            const Rank rank = Rank(kingPositionsStart[color].rowIdx);
            if (color == BLACK)
                charPiece = tolower(charPiece);
            if (!board.is_piece_on_rank(charPiece, rank)) {
                std::cerr << "The " << color_to_string(color) << " king and rook must be on rank " << rank << " if castling is enabled for " << color_to_string(color) << "." << std::endl;
                return NOK;
            }
        }
    }
    return OK;
}

std::string castling_rights_to_string(CastlingRights castlingRights) {
    switch (castlingRights) {
    case KING_SIDE:
        return "KING_SIDE";
    case QUEEN_SIDE:
        return "QUEENS_SIDE";
    case WHITE_OO:
        return "WHITE_OO";
    case WHITE_OOO:
        return "WHITE_OOO";
    case BLACK_OO:
        return "BLACK_OO";
    case BLACK_OOO:
        return "BLACK_OOO";
    case WHITE_CASTLING:
        return "WHITE_CASTLING";
    case BLACK_CASTLING:
        return "BLACK_CASTLING";
    case ANY_CASTLING:
        return "ANY_CASTLING";
    case CASTLING_RIGHT_NB:
        return "CASTLING_RIGHT_NB";
    default:
        return "INVALID_CASTLING_RIGHTS";
    }
}

Validation check_touching_kings(const CharBoard& board, const std::array<CharSquare, 2>& kingPositions) {
    if (non_root_euclidian_distance(kingPositions[WHITE], kingPositions[BLACK]) <= 2) {
        std::cerr << "King pieces are next to each other." << std::endl;
        std::cerr << board << std::endl;
        return NOK;
    }
    return OK;
}

Validation check_standard_castling(std::array<std::string, 2>& castlingInfoSplitted, const CharBoard& board,
                             const std::array<CharSquare, 2>& kingPositions, const std::array<CharSquare, 2>& kingPositionsStart,
                             const std::array<std::vector<CharSquare>, 2>& rookPositionsStart) {

    for (Color c : {WHITE, BLACK}) {
        if (castlingInfoSplitted[c].size() == 0)
            continue;
        if (kingPositions[c] != kingPositionsStart[c]) {
            std::cerr << "The " << color_to_string(c) << " KING has moved. Castling is no longer valid for " << color_to_string(c) << "." << std::endl;
            return NOK;
        }

        for (CastlingRights castling: {KING_SIDE, QUEEN_SIDE}) {
            CharSquare rookStartingSquare = castling == QUEEN_SIDE ? rookPositionsStart[c][0] : rookPositionsStart[c][1];
            char targetChar = castling == QUEEN_SIDE ? 'q' : 'k';
            char rookChar = 'R'; // we don't use v->pieceToChar[ROOK]; here because in the newzealand_variant the ROOK is replaced by ROOKNI
            if (c == BLACK)
                rookChar = tolower(rookChar);
            if (castlingInfoSplitted[c].find(targetChar) != std::string::npos) {
                if (board.get_piece(rookStartingSquare.rowIdx, rookStartingSquare.fileIdx) != rookChar) {
                    std::cerr << "The " << color_to_string(c) << " ROOK on the "<<  castling_rights_to_string(castling) << " has moved. "
                              << castling_rights_to_string(castling) << " castling is no longer valid for " << color_to_string(c) << "." << std::endl;
                    return NOK;
                }
            }

        }
    }
    return OK;
}

Validation check_pocket_info(const std::string& fenBoard, int nbRanks, const Variant* v, std::array<std::string, 2>& pockets) {

    char stopChar;
    int offset = 0;
    if (std::count(fenBoard.begin(), fenBoard.end(), '/') == nbRanks) {
        // look for last '/'
        stopChar = '/';
    }
    else {
        // pocket is defined as [ and ]
        stopChar = '[';
        offset = 1;
        if (*(fenBoard.end()-1) != ']') {
            std::cerr << "Pocket specification does not end with ']'." << std::endl;
            return NOK;
        }
    }

    // look for last '/'
    for (auto it = fenBoard.rbegin()+offset; it != fenBoard.rend(); ++it) {
        const char c = *it;
        if (c == stopChar)
            return OK;
        if (c != '-') {
            if (v->pieceToChar.find(c) == std::string::npos) {
                std::cerr << "Invalid pocket piece: '" << c << "'." << std::endl;
                return NOK;
            }
            else {
                if (isupper(c))
                    pockets[WHITE] += tolower(c);
                else
                    pockets[BLACK] += c;
            }
        }
    }
    std::cerr << "Pocket piece closing character '" << stopChar << "' was not found." << std::endl;
    return NOK;
}

Validation check_number_of_kings(const std::string& fenBoard, const Variant* v) {
    int nbWhiteKings = std::count(fenBoard.begin(), fenBoard.end(), toupper(v->pieceToChar[KING]));
    int nbBlackKings = std::count(fenBoard.begin(), fenBoard.end(), tolower(v->pieceToChar[KING]));

    if (nbWhiteKings != 1) {
        std::cerr << "Invalid number of white kings. Expected: 1. Given: " << nbWhiteKings << std::endl;
        return NOK;
    }
    if (nbBlackKings != 1) {
        std::cerr << "Invalid number of black kings. Expected: 1. Given: " << nbBlackKings << std::endl;
        return NOK;
    }
    return OK;
}

Validation check_en_passant_square(const std::string& enPassantInfo) {
    const char firstChar = enPassantInfo[0];
    if (firstChar != '-') {
        if (enPassantInfo.size() != 2) {
            std::cerr << "Invalid en-passant square '" << enPassantInfo << "'. Expects 2 characters. Actual: " << enPassantInfo.size() << " character(s)." << std::endl;
            return NOK;
        }
        if (isdigit(firstChar)) {
            std::cerr << "Invalid en-passant square '" << enPassantInfo << "'. Expects 1st character to be a digit." << std::endl;
            return NOK;
        }
        const char secondChar = enPassantInfo[1];
        if (!isdigit(secondChar)) {
            std::cerr << "Invalid en-passant square '" << enPassantInfo << "'. Expects 2nd character to be a non-digit." << std::endl;
            return NOK;
        }
    }
    return OK;
}

bool no_king_piece_in_pockets(const std::array<std::string, 2>& pockets) {
    return pockets[WHITE].find('k') == std::string::npos && pockets[BLACK].find('k') == std::string::npos;
}

Validation check_digit_field(const std::string& field)
{
    if (field.size() == 1 && field[0] == '-') {
        return OK;
    }
    for (char c : field)
        if (!isdigit(c)) {
            return NOK;
        }
    return OK;
}


FenValidation validate_fen(const std::string& fen, const Variant* v) {

    const std::string validSpecialCharacters = "/+~[]-";
    // 0) Layout
    // check for empty fen
    if (fen.size() == 0) {
        std::cerr << "Fen is empty." << std::endl;
        return FEN_EMPTY;
    }

    // check for space
    if (fen.find(' ') == std::string::npos) {
        std::cerr << "Fen misses space as delimiter." << std::endl;
        return FEN_MISSING_SPACE_DELIM;
    }

    std::vector<std::string> fenParts = get_fen_parts(fen, ' ');
    std::vector<std::string> starFenParts = get_fen_parts(v->startFen, ' ');
    const unsigned int nbFenParts = starFenParts.size();

    // check for number of parts (also up to two additional "-" for non-existing no-progress counter or castling rights)
    const unsigned int maxNumberFenParts = 7U;
    const unsigned int topThreshold = std::min(nbFenParts + 2, maxNumberFenParts);
    if (fenParts.size() < nbFenParts || fenParts.size() > topThreshold) {
        std::cerr << "Invalid number of fen parts. Expected: >= " << nbFenParts << " and <= " << topThreshold
                  << " Actual: " << fenParts.size() << std::endl;
        return FEN_INVALID_NB_PARTS;
    }

    // 1) Part
    // check for valid characters
    if (check_for_valid_characters(fenParts[0], validSpecialCharacters, v) == NOK) {
        return FEN_INVALID_CHAR;
    }

    // check for number of ranks
    const int nbRanks = v->maxRank + 1;
    // check for number of files
    const int nbFiles = v->maxFile + 1;
    CharBoard board(nbRanks, nbFiles);  // create a 2D character board for later geometry checks

    if (fill_char_board(board, fenParts[0], validSpecialCharacters, v) == NOK)
        return FEN_INVALID_BOARD_GEOMETRY;

    // check for pocket
    std::array<std::string, 2> pockets;
    if (v->pieceDrops) {
        if (check_pocket_info(fenParts[0], nbRanks, v, pockets) == NOK)
            return FEN_INVALID_POCKET_INFO;
    }

    // check for number of kings (skip all extinction variants for this check (e.g. horde is a sepcial case where only one side has a royal king))
    if (v->pieceTypes.find(KING) != v->pieceTypes.end() && v->extinctionPieceTypes.size() == 0) {
        // we have a royal king in this variant, ensure that each side has exactly one king
        // (variants like giveaway use the COMMONER piece type instead)
        if (check_number_of_kings(fenParts[0], v) == NOK)
            return FEN_INVALID_NUMBER_OF_KINGS;

        // if kings are still in pockets skip this check (e.g. placement_variant)
        if (no_king_piece_in_pockets(pockets)) {
            // check if kings are touching
            std::array<CharSquare, 2> kingPositions;
            // check if kings are touching
            kingPositions[WHITE] = board.get_square_for_piece(toupper(v->pieceToChar[KING]));
            kingPositions[BLACK] = board.get_square_for_piece(tolower(v->pieceToChar[KING]));
            if (check_touching_kings(board, kingPositions) == NOK)
                return FEN_TOUCHING_KINGS;

            // 3) Part
            // check castling rights
            if (v->castling) {
                std::array<std::string, 2> castlingInfoSplitted;
                if (fill_castling_info_splitted(fenParts[2], castlingInfoSplitted) == NOK)
                    return FEN_INVALID_CASTLING_INFO;

                if (castlingInfoSplitted[WHITE].size() != 0 || castlingInfoSplitted[BLACK].size() != 0) {

                    CharBoard startBoard(board.get_nb_ranks(), board.get_nb_files());
                    fill_char_board(startBoard, v->startFen, validSpecialCharacters, v);
                    std::array<CharSquare, 2> kingPositionsStart;
                    kingPositionsStart[WHITE] = startBoard.get_square_for_piece(toupper(v->pieceToChar[KING]));
                    kingPositionsStart[BLACK] = startBoard.get_square_for_piece(tolower(v->pieceToChar[KING]));

                    if (v->chess960) {
                        if (check_960_castling(castlingInfoSplitted, board, kingPositionsStart) == NOK)
                            return FEN_INVALID_CASTLING_INFO;
                    }
                    else {
                        std::array<std::vector<CharSquare>, 2> rookPositionsStart;
                        // we don't use v->pieceToChar[ROOK]; here because in the newzealand_variant the ROOK is replaced by ROOKNI
                        rookPositionsStart[WHITE] = startBoard.get_squares_for_piece('R');
                        rookPositionsStart[BLACK] = startBoard.get_squares_for_piece('r');

                        if (check_standard_castling(castlingInfoSplitted, board, kingPositions, kingPositionsStart, rookPositionsStart) == NOK)
                            return FEN_INVALID_CASTLING_INFO;
                    }
                }

            }
        }
    }

    // 2) Part
    // check side to move char
    if (fenParts[1][0] != 'w' && fenParts[1][0] != 'b') {
        std::cerr << "Invalid side to move specification: '" << fenParts[1][0] << "'." << std::endl;
        return FEN_INVALID_SIDE_TO_MOVE;
    }

    // 4) Part
    // check en-passant square
    if (v->doubleStep && v->pieceTypes.find(PAWN) != v->pieceTypes.end()) {
        if (check_en_passant_square(fenParts[3]) == NOK)
            return FEN_INVALID_EN_PASSANT_SQ;
    }

    // 5) Part
    // checkCounting is skipped because if only one check is required to win it must not be part of the FEN (e.g. karouk_variant)

    // 6) Part
    // check half move counter
    if (!check_digit_field(fenParts[fenParts.size()-2])) {
        std::cerr << "Invalid half move counter: '" << fenParts[fenParts.size()-2] << "'." << std::endl;
        return FEN_INVALID_HALF_MOVE_COUNTER;
    }

    // 7) Part
    // check move counter
    if (!check_digit_field(fenParts[fenParts.size()-1])) {
        std::cerr << "Invalid move counter: '" << fenParts[fenParts.size()-1] << "'." << std::endl;
        return FEN_INVALID_MOVE_COUNTER;
    }

    return FEN_OK;
}
}