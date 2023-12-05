#include <SFML/Graphics.hpp>
#include <cstdint>
#include <optional>
#include <vector>
#include <stack>
#include <forward_list>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <sstream>
#include <cctype>

#include "precomputed.hpp"

#define LIGHT_SQUARE_COLOR sf::Color(0xf0, 0xd9, 0xb5)
#define DARK_SQUARE_COLOR sf::Color(0xb5, 0x88, 0x63)

#define LIGHT_CURRENTLY_SELECTED sf::Color(0xdc, 0xc3, 0x4b)
#define DARK_CURRENTLY_SELECTED LIGHT_CURRENTLY_SELECTED

#define LIGHT_AVAILABLE_TARGET LIGHT_SQUARE_COLOR * sf::Color(210, 210, 200)
#define DARK_AVAILABLE_TARGET DARK_SQUARE_COLOR * sf::Color(200, 200, 200)

#define LIGHT_PREVIOUS_MOVE sf::Color(0xA0, 0xD0, 0xE0) * sf::Color(200, 200, 200)
#define DARK_PREVIOUS_MOVE LIGHT_PREVIOUS_MOVE

typedef std::uint_fast64_t uint64;

class DrawableBoard : public sf::Drawable
{
public:
    DrawableBoard(sf::Vector2f position, bool whiteOnBottom)
    {
        boardPosition = position;
        bottomIsWhite = whiteOnBottom;
        
        // Load peice textures
        sf::Image empty;
        empty.create(120, 120, sf::Color(0, 0, 0, 0));
        peiceTextures[0].loadFromImage(empty);
        peiceTextures[1].loadFromFile("assets/120px/white_pawn.png");
        peiceTextures[2].loadFromFile("assets/120px/white_knight.png");
        peiceTextures[3].loadFromFile("assets/120px/white_bishop.png");
        peiceTextures[4].loadFromFile("assets/120px/white_rook.png");
        peiceTextures[5].loadFromFile("assets/120px/white_queen.png");
        peiceTextures[6].loadFromFile("assets/120px/white_king.png");
        peiceTextures[7].loadFromImage(empty);
        peiceTextures[8].loadFromImage(empty);
        peiceTextures[9].loadFromFile("assets/120px/black_pawn.png");
        peiceTextures[10].loadFromFile("assets/120px/black_knight.png");
        peiceTextures[11].loadFromFile("assets/120px/black_bishop.png");
        peiceTextures[12].loadFromFile("assets/120px/black_rook.png");
        peiceTextures[13].loadFromFile("assets/120px/black_queen.png");
        peiceTextures[14].loadFromFile("assets/120px/black_king.png");

        hoveringPeice.setTexture(peiceTextures[0]);
        hoveringPeice.setOrigin(60, 60);

        initialize("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", whiteOnBottom);
    }
    
    sf::Vector2f getPosition()
    {
        return boardPosition;
    }
    
    void mouseDrag(sf::Vector2f position)
    {
        hoveringPeice.setPosition(position);
    }

    void mouseDown(sf::Vector2f position)
    {
        sf::Vector2f relativePosition = position - boardPosition;
        int x = static_cast<int>(relativePosition.x) / 120;
        int y = static_cast<int>(relativePosition.y) / 120;
        if (x < 0 || x > 7 || y < 0 || y > 7) {
            currentlySelected.reset();
            resetSquareHighlights();
            return;
        }
        int index = bottomIsWhite ? (7 - y) * 8 + x : y * 8 + (7 - x);

        
        if (peices[index] && peices[index] >> 3 == totalHalfmoves % 2) {
            // Selected a new peice
            currentlySelected = index;
            hoveringPeice.setTexture(peiceTextures[peices[index]]);
            peiceSprites[index].setTexture(peiceTextures[0]);
            hoveringPeice.setPosition(position);
            resetSquareHighlights();
            return;
        
        } else if (currentlySelected.has_value()) {
            // Selecting a target for the peice
            int s = currentlySelected.value();
            for (Move &move : currentLegalMoves) {
                if (move.start() == s && move.target() == index) {
                    makeMove(move);
                    // Clear hovering peice
                    peiceSprites[s].setTexture(peiceTextures[peices[s]]);
                    hoveringPeice.setTexture(peiceTextures[0]);
                    currentlySelected.reset();
                    resetSquareHighlights();
                    return;
                }
            }
        }

        resetSquareHighlights();
    }

    void mouseUp(sf::Vector2f position)
    {
        if (!currentlySelected.has_value()) {
            // No peice to move
            return;
        }

        int s = currentlySelected.value();

        // Clear hovering peice
        peiceSprites[s].setTexture(peiceTextures[peices[s]]);
        hoveringPeice.setTexture(peiceTextures[0]);

        sf::Vector2f relativePosition = position - boardPosition;
        int x = static_cast<int>(relativePosition.x) / 120;
        int y = static_cast<int>(relativePosition.y) / 120;
        if (x < 0 || x > 7 || y < 0 || y > 7) {
            currentlySelected.reset();
            resetSquareHighlights();
            return;
        }
        int index = bottomIsWhite ? (7 - y) * 8 + x : y * 8 + (7 - x);

        // Selecting a target for the peice
        for (Move &move : currentLegalMoves) {
            if (move.start() == s && move.target() == index) {
                makeMove(move);
                currentlySelected.reset();
                resetSquareHighlights();
                return;
            }
        }

        resetSquareHighlights();
    }

    int colorToMove() noexcept
    {
        return 1 - 2 * (totalHalfmoves % 2);
    }
    
    std::optional<int> gameOver() noexcept
    {
        if (isDraw()) {
            return 0;
        }

        if (currentLegalMoves.size() == 0) {
            return inCheck() ? -colorToMove() : 0;
        }

        return std::nullopt;
    }
    

private:    
    // DEFINITIONS
    static constexpr int WHITE = 0b0000;
    static constexpr int BLACK = 0b1000;
    static constexpr int PAWN =   0b001;
    static constexpr int KNIGHT = 0b010;
    static constexpr int BISHOP = 0b011;
    static constexpr int ROOK =   0b100;
    static constexpr int QUEEN =  0b101;
    static constexpr int KING =   0b110;


    // MOVE
    // struct for containing info about a move
    class Move
    {   
    public:
        // Starting square of the move [0, 63] -> [a1, h8]
        inline int start() const noexcept
        {
            return startSquare;
        }
        
        // Ending square of the move [0, 63] -> [a1, h8]
        inline int target() const noexcept
        {
            return targetSquare;
        }

        // Peice and color of moving peice (peice that is on the starting square of the move
        inline int moving() const noexcept
        {
            return movingPeice;
        }

        // Peice and color of captured peice
        inline int captured() const noexcept
        {
            return capturedPeice;
        }

        // Returns the color of the whose move is
        inline int color() const noexcept
        {
            return (movingPeice >> 3) << 3;
        }

        // Returs the color who the move is being played against
        inline int enemy() const noexcept
        {
            return !(movingPeice >> 3) << 3;
        }

        // In case of promotion, returns the peice value of the promoted peice
        inline int promotion() const noexcept
        {
            return flags & PROMOTION;
        }

        // Returns true if move is castling move
        inline bool isEnPassant() const noexcept
        {
            return flags & EN_PASSANT;
        }

        // Returns true if move is en passant move
        inline bool isCastling() const noexcept
        {
            return flags & CASTLE;
        }

        // Override equality operator with other move
        inline bool operator==(const Move& other) const
        {
            return this->start() == other.start()
                && this->target() == other.target()
                && this->flags & PROMOTION == other.flags & PROMOTION;
        }

        // CONSTRUCTORS
        // Construct a new Move object from the given board and given flags (en_passant, castle, promotion, etc.)
        Move(const DrawableBoard *board, int start, int target, int givenFlags=NONE) : startSquare(start), targetSquare(target), flags(givenFlags)
        {
            movingPeice = board->peices[start];
            capturedPeice = board->peices[target];
            if (isEnPassant()) {
                capturedPeice = enemy() + PAWN;
            }
        }

        Move() : startSquare(0), targetSquare(0), movingPeice(0), capturedPeice(0), flags(0) {}
        
        // FLAGS
        static constexpr int NONE       = 0b00000;
        static constexpr int PROMOTION  = 0b00111;
        static constexpr int EN_PASSANT = 0b01000;
        static constexpr int CASTLE     = 0b10000;
    private:
        // Starting square of the move [0, 63] -> [a1, h8]
        int startSquare;
        
        // Ending square of the move [0, 63] -> [a1, h8]
        int targetSquare;

        // Peice and color of moving peice (peice that is on the starting square of the move
        int movingPeice;

        // Peice and color of captured peice
        int capturedPeice;

        // 8 bit flags of move
        int flags;
   };


    // GRAPHICAL MEMBERS
    sf::Vector2f boardPosition;

    sf::Texture peiceTextures[15];

    sf::Sprite peiceSprites[64];

    sf::RectangleShape squareSprites[64];

    sf::Sprite hoveringPeice;

    bool bottomIsWhite;


    // INTERFACE MEMBERS
    std::optional<int> currentlySelected;

    std::stack<Move> previousMove;

    // Legal moves for the current position stored in the engine
    std::vector<Move> currentLegalMoves;


    // BOARD MEMBERS
    // color and peice type at every square (index [0, 63] -> [a1, h8])
    int peices[64];

    // contains the halfmove number when the kingside castling rights were lost for white or black (index 0 and 1)
    int kingsideCastlingRightsLost[2];

    // contains the halfmove number when the queenside castling rights were lost for white or black (index 0 and 1)
    int queensideCastlingRightsLost[2];

    // file where a pawn has just moved two squares over
    std::stack<int> eligibleEnPassantSquare;

    // number of half moves since pawn move or capture (half move is one player taking a turn) (used for 50 move rule)
    std::stack<int> halfmovesSincePawnMoveOrCapture;

    // total half moves since game start (half move is one player taking a turn)
    int totalHalfmoves;

    // index of the white and black king (index 0 and 1)
    int kingIndex[2];

    uint64 zobrist;

    // array of 32 bit hashes of the positions used for checking for repititions
    std::forward_list<uint64> positionHistory;


    // BOARD METHODS
    // Initialize engine members for position
    void initialize(const std::string &fenString, bool whiteOnBottom)
    {
        // Reset current members
        zobrist = 0;
        previousMove = std::stack<Move>();
        currentlySelected.reset();
        
        std::istringstream fenStringStream(fenString);
        std::string peicePlacementData, activeColor, castlingAvailabilty, enPassantTarget, halfmoveClock, fullmoveNumber;
        
        // Get peice placement data from fen string
        if (!std::getline(fenStringStream, peicePlacementData, ' ')) {
            throw std::invalid_argument("Cannot get peice placement from FEN!");
        }

        // Update the peices[] according to fen rules
        int peiceIndex = 56;
        for (char peiceInfo : peicePlacementData) {
            if (std::isalpha(peiceInfo)) {
                // char contains data about a peice
                //int color = std::islower(c);
                int c = peiceInfo > 96 && peiceInfo < 123;
                int color = c << 3;

                switch (peiceInfo) {
                    case 'P':
                    case 'p':
                        peices[peiceIndex++] = color + PAWN;
                        break;
                    case 'N':
                    case 'n':
                        peices[peiceIndex++] = color + KNIGHT;
                        break;
                    case 'B':
                    case 'b':
                        peices[peiceIndex++] = color + BISHOP;
                        break;
                    case 'R':
                    case 'r':
                        peices[peiceIndex++] = color + ROOK;
                        break;
                    case 'Q':
                    case 'q':
                        peices[peiceIndex++] = color + QUEEN;
                        break;
                    case 'K':
                    case 'k':
                        kingIndex[color >> 3] = peiceIndex;
                        peices[peiceIndex++] = color + KING;
                        break;
                    default:
                        throw std::invalid_argument("Unrecognised alpha char in FEN peice placement data!");
                }

            } else if (std::isdigit(peiceInfo)) {
                // char contains data about gaps between peices
                int gap = peiceInfo - '0';
                for (int i = 0; i < gap; ++i) {
                    peices[peiceIndex++] = 0;
                }

            } else {
                if (peiceInfo != '/') {
                    // Only "123456789pnbrqkPNBRQK/" are allowed in peice placement data
                    throw std::invalid_argument("Unrecognised char in FEN peice placement data!");
                }

                if (peiceIndex % 8 != 0) {
                    // Values between '/' should add up to 8
                    throw std::invalid_argument("Arithmetic error in FEN peice placement data!");
                }

                // Move peice index to next rank
                peiceIndex -= 16;
            }
        }

        // Get active color data from fen string
        if (!std::getline(fenStringStream, activeColor, ' ')) {
            throw std::invalid_argument("Cannot get active color from FEN!");
        }

        if (activeColor == "w") {
            // White is to move
            totalHalfmoves = 0;

        } else if (activeColor == "b") {
            // Black is to move
            totalHalfmoves = 1;
            zobrist ^= ZOBRIST_TURN_KEY;
        
        } else {
            // active color can only be "wb"
            throw std::invalid_argument("Unrecognised charecter in FEN active color");
        }

        // Get castling availability data from fen string
        if (!std::getline(fenStringStream, castlingAvailabilty, ' ')) {
            throw std::invalid_argument("Cannot get castling availability from FEN!");
        }

        // Update castling availibility according to fen rules
        kingsideCastlingRightsLost[0] = -1;
        kingsideCastlingRightsLost[1] = -1;
        queensideCastlingRightsLost[0] = -1;
        queensideCastlingRightsLost[1] = -1;

        if (castlingAvailabilty != "-") {
            for (char castlingInfo : castlingAvailabilty) {
                //int color = std::islower(c);
                int c = castlingInfo > 96 && castlingInfo < 123;
                int color = c << 3;
                int castlingRank = 56 * c;
                switch (castlingInfo) {
                    case 'K':
                    case 'k':
                        if (peices[castlingRank + 4] == color + KING && peices[castlingRank + 7] == color + ROOK) {
                            kingsideCastlingRightsLost[c] = 0;
                            zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
                        }
                        break;
                    case 'Q':
                    case 'q':
                        if (peices[castlingRank + 4] == color + KING && peices[castlingRank] == color + ROOK) {
                            queensideCastlingRightsLost[c] = 0;
                            zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];
                        }
                        break;
                    default:
                        throw std::invalid_argument("Unrecognised char in FEN castling availability data!");
                }
            }
        }

        // Get en passant target data from fen string
        if (!std::getline(fenStringStream, enPassantTarget, ' ')) {
            throw std::invalid_argument("Cannot get en passant target from FEN!");
        }

        if (enPassantTarget != "-") {
            try {
                eligibleEnPassantSquare.push(algebraicNotationToBoardIndex(enPassantTarget));
            } catch (const std::invalid_argument &e) {
                throw std::invalid_argument(std::string("Invalid FEN en passant target! ") + e.what());
            }
        
        } else {
            eligibleEnPassantSquare.push(-1);
        }

        // Get half move clock data from fen string
        if (!std::getline(fenStringStream, halfmoveClock, ' ')) {
            halfmoveClock = "0";
            //throw std::invalid_argument("Cannot get halfmove clock from FEN!");
        }

        try {
            halfmovesSincePawnMoveOrCapture.push(static_cast<int>(std::stoi(halfmoveClock)));
        } catch (const std::invalid_argument &e) {
            throw std::invalid_argument(std::string("Invalid FEN half move clock! ") + e.what());
        }
        // prevent out of bounds error when searching for repititions
        for (int i = 0; i < halfmovesSincePawnMoveOrCapture.top(); ++i) {
            positionHistory.push_front(0);
        }

        // Get full move number data from fen string
        if (!std::getline(fenStringStream, fullmoveNumber, ' ')) {
            fullmoveNumber = "1";
            //throw std::invalid_argument("Cannot getfullmove number from FEN!");
        }

        try {
            totalHalfmoves += static_cast<int>(std::stoi(fullmoveNumber) * 2 - 2);
        } catch (const std::invalid_argument &e) {
            throw std::invalid_argument(std::string("Invalid FEN full move number! ") + e.what());
        }

        // initialize zobrist hash and set sprites for all of the peices and squares
        for (int i = 0; i < 64; ++i) {
            int peice = peices[i];

            int file = i % 8;
            int rank = i / 8;
            peiceSprites[i].setTexture(peiceTextures[peice]);
            peiceSprites[i].setPosition(boardPosition + (whiteOnBottom ? sf::Vector2f(file * 120, (7 - rank) * 120) : sf::Vector2f((7 - file) * 120, rank * 120)));
            
            squareSprites[i] = sf::RectangleShape(sf::Vector2f(120, 120));
            squareSprites[i].setPosition(boardPosition + (whiteOnBottom ? sf::Vector2f(file * 120, (7 - rank) * 120) : sf::Vector2f((7 - file) * 120, rank * 120)));
            
            if (peice) {
                zobrist ^= ZOBRIST_PEICE_KEYS[peice >> 3][peice % (1 << 3) - 1][i];
            }
        }

        resetSquareHighlights();

        currentLegalMoves = legalMoves();
    }

    // Generates pseudo legal moves for the current position
    std::vector<Move> pseudoLegalMoves() const
    {
        int c = totalHalfmoves % 2;
        int color = c << 3;
        int e = !color;
        int enemy = e << 3;

        std::vector<Move> moves;

        // General case
        for (int s = 0; s < 64; ++s) {
            if (peices[s] && peices[s] >> 3 == c) {

                switch (peices[s] % (1 << 3)) {
                    case PAWN: {
                        int file = s % 8;
                        int ahead = s + 8 - 16 * c;
                        bool promotion = color == WHITE ? (s >> 3 == 6) : (s >> 3 == 1);

                        // Pawn foward moves
                        if (!peices[ahead]) {
                            if (promotion) {
                                moves.emplace_back(this, s, ahead, KNIGHT);
                                moves.emplace_back(this, s, ahead, BISHOP);
                                moves.emplace_back(this, s, ahead, ROOK);
                                moves.emplace_back(this, s, ahead, QUEEN);
                            } else {
                                moves.emplace_back(this, s, ahead);
                            }

                            bool doubleJumpAllowed = color == WHITE ? (s >> 3 == 1) : (s >> 3 == 6);
                            int doubleAhead = ahead + 8 - 16 * c;
                            if (doubleJumpAllowed && !peices[doubleAhead]) {
                                moves.emplace_back(this, s, doubleAhead);
                            }
                        }

                        // Pawn captures
                        if (file != 0 && peices[ahead - 1] && peices[ahead - 1] >> 3 == e) {
                            if (promotion) {
                                moves.emplace_back(this, s, ahead - 1, KNIGHT);
                                moves.emplace_back(this, s, ahead - 1, BISHOP);
                                moves.emplace_back(this, s, ahead - 1, ROOK);
                                moves.emplace_back(this, s, ahead - 1, QUEEN);
                            } else {
                                moves.emplace_back(this, s, ahead - 1);
                            }
                        }
                        if (file != 7 && peices[ahead + 1] && peices[ahead + 1] >> 3 == e) {
                            if (promotion) {
                                moves.emplace_back(this, s, ahead + 1, KNIGHT);
                                moves.emplace_back(this, s, ahead + 1, BISHOP);
                                moves.emplace_back(this, s, ahead + 1, ROOK);
                                moves.emplace_back(this, s, ahead + 1, QUEEN);
                            } else {
                                moves.emplace_back(this, s, ahead + 1);
                            }
                        }
                        break;
                    }
                    case KNIGHT: {
                        int t;
                        for (int j = 1; j < KNIGHT_MOVES[s][0]; ++j) {
                            t = KNIGHT_MOVES[s][j];
                            if ((!peices[t] || peices[t] >> 3 == e)) {
                                moves.emplace_back(this, s, t);
                            }
                        }
                        break;
                    }
                    case BISHOP:
                    case ROOK:
                    case QUEEN: {
                        if (peices[s] % (1 << 3) == BISHOP) {
                            goto bishopMoves;
                        }

                        for (int t = s - 8; t >= DIRECTION_BOUNDS[s][B]; t -= 8) {
                            if ((!peices[t] || peices[t] >> 3 == e)) {
                                moves.emplace_back(this, s, t);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        for (int t = s + 8; t <= DIRECTION_BOUNDS[s][F]; t += 8) {
                            if ((!peices[t] || peices[t] >> 3 == e)) {
                                moves.emplace_back(this, s, t);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        for (int t = s - 1; t >= DIRECTION_BOUNDS[s][L]; t -= 1) {
                            if ((!peices[t] || peices[t] >> 3 == e)) {
                                moves.emplace_back(this, s, t);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        for (int t = s + 1; t <= DIRECTION_BOUNDS[s][R]; t += 1) {
                            if ((!peices[t] || peices[t] >> 3 == e)) {
                                moves.emplace_back(this, s, t);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        if (peices[s] % (1 << 3) == ROOK) {
                            break;
                        }
                        bishopMoves:

                        for (int t = s - 9; t >= DIRECTION_BOUNDS[s][BL]; t -= 9) {
                            if ((!peices[t] || peices[t] >> 3 == e)) {
                                moves.emplace_back(this, s, t);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        for (int t = s + 9; t <= DIRECTION_BOUNDS[s][FR]; t += 9) {
                            if ((!peices[t] || peices[t] >> 3 == e)) {
                                moves.emplace_back(this, s, t);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        for (int t = s - 7; t >= DIRECTION_BOUNDS[s][BR]; t -= 7) {
                            if ((!peices[t] || peices[t] >> 3 == e)) {
                                moves.emplace_back(this, s, t);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }

                        for (int t = s + 7; t <= DIRECTION_BOUNDS[s][FL]; t += 7) {
                            if ((!peices[t] || peices[t] >> 3 == e)) {
                                moves.emplace_back(this, s, t);
                            }
                            if (peices[t]) {
                                break;
                            }
                        }
                        
                        break;
                    }
                    case KING: {
                        int t;
                        for (int j = 1; j < KING_MOVES[s][0]; ++j) {
                            t = KING_MOVES[s][j];
                            if (!peices[t] || peices[t] >> 3 == e) {
                                moves.emplace_back(this, s, t);
                            }
                        }
                    }
                }
            }
        }

        // Castling moves
        if (!kingsideCastlingRightsLost[c]) {
            int castlingRank = 56 * c;
            bool roomToCastle = true;
            for (int j = castlingRank + 5; j < castlingRank + 7; ++j) {
                if (peices[j]) {
                    roomToCastle = false;
                    break;
                }
            }
            if (roomToCastle) {
                moves.emplace_back(this, castlingRank + 4, castlingRank + 6, Move::CASTLE);
            }
        }
        if (!queensideCastlingRightsLost[c]) {
            int castlingRank = 56 * c;
            bool roomToCastle = true;
            for (int j = castlingRank + 3; j > castlingRank; --j) {
                if (peices[j]) {
                    roomToCastle = false;
                    break;
                }
            }
            if (roomToCastle) {
                moves.emplace_back(this, castlingRank + 4, castlingRank + 2, Move::CASTLE);
            }
        }

        // En passant moves
        int epSquare = eligibleEnPassantSquare.top();
        if (epSquare >= 0) {
            int epfile = epSquare % 8;
            if (color == WHITE) {
                if (epfile != 0 && peices[epSquare - 9] == color + PAWN) {
                    moves.emplace_back(this, epSquare - 9, epSquare, Move::EN_PASSANT);
                }
                if (epfile != 7 && peices[epSquare - 7] == color + PAWN) {
                    moves.emplace_back(this, epSquare - 7, epSquare, Move::EN_PASSANT);
                }
            } else {
                if (epfile != 0 && peices[epSquare + 7] == color + PAWN) {
                    moves.emplace_back(this, epSquare + 7, epSquare, Move::EN_PASSANT);
                }
                if (epfile != 7 && peices[epSquare + 9] == color + PAWN) {
                    moves.emplace_back(this, epSquare + 9, epSquare, Move::EN_PASSANT);
                }
            }
        }

        return moves;
    }
    
    // Generates legal moves for the current position
    std::vector<Move> legalMoves()
    {
        std::vector<Move> moves = pseudoLegalMoves();
        moves.erase(std::remove_if(moves.begin(), moves.end(), [&](Move m) { return !isLegal(m); }), moves.end());
        return moves;
    }
    
    // update the board based on the inputted move (must be legal)
    void makeMove(Move &move)
    {
        positionHistory.push_front(zobrist);
        
        int c = move.moving() >> 3;
        int color = c << 3;
        int e = !color;
        int enemy = e << 3;
        
        // UPDATE PEICE DATA / ZOBRIST HASH
        // Update zobrist hash for turn change
        zobrist ^= ZOBRIST_TURN_KEY;

        // Update peices array and zobrist hash for moving peice
        peices[move.start()] = 0;
        peiceSprites[move.start()].setTexture(peiceTextures[0]);
        zobrist ^= ZOBRIST_PEICE_KEYS[c][move.moving() % (1 << 3) - 1][move.start()];

        if (move.promotion()) {
            peices[move.target()] = color + move.promotion();
            peiceSprites[move.target()].setTexture(peiceTextures[color + move.promotion()]);
            zobrist ^= ZOBRIST_PEICE_KEYS[c][move.promotion() - 1][move.target()];
            
        } else {
            peices[move.target()] = move.moving();
            peiceSprites[move.target()].setTexture(peiceTextures[move.moving()]);
            zobrist ^= ZOBRIST_PEICE_KEYS[c][move.moving() % (1 << 3) - 1][move.target()];
        }
        
        // Update zobrist hash and peice indices set for capture
        if (move.isEnPassant()) {
            int captureSquare = move.target() - 8 + 16 * c;
            peices[captureSquare] = 0;
            peiceSprites[captureSquare].setTexture(peiceTextures[0]);
            zobrist ^= ZOBRIST_PEICE_KEYS[e][move.captured() % (1 << 3) - 1][captureSquare];

        } else {
            zobrist ^= ZOBRIST_PEICE_KEYS[e][move.captured() % (1 << 3) - 1][move.target()];
        }

        // Update rooks for castling
        if (move.isCastling()) {
            int rookStart;
            int rookEnd;

            int castlingRank = move.target() & 0b11111000;
            if (move.target() % 8 < 4) {
                // Queenside castling
                rookStart = castlingRank;
                rookEnd = castlingRank + 3;
            } else {
                // Kingside castling
                rookStart = castlingRank + 7;
                rookEnd = castlingRank + 5;
            }

            peices[rookEnd] = peices[rookStart];
            peiceSprites[rookEnd].setTexture(peiceTextures[peices[rookStart]]);

            peices[rookStart] = 0;
            peiceSprites[rookStart].setTexture(peiceTextures[0]);

            zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][rookStart];
            zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][rookEnd];
        }

        // UPDATE BOARD FLAGS
        // Update king index
        if (move.moving() % (1 << 3) == KING) {
            kingIndex[c] = move.target();
        }

        // increment counters / Update position history
        ++totalHalfmoves;
        if (move.captured() || move.moving() == color + PAWN) {
            halfmovesSincePawnMoveOrCapture.push(0);
        } else {
            int prev = halfmovesSincePawnMoveOrCapture.top();
            halfmovesSincePawnMoveOrCapture.pop();
            halfmovesSincePawnMoveOrCapture.push(prev + 1);
        }

        // En passant file
        if (move.moving() % (1 << 3) == PAWN && std::abs(move.target() - move.start()) == 16) {
            eligibleEnPassantSquare.push((move.start() + move.target()) / 2);
        } else {
            eligibleEnPassantSquare.push(-1);
        }       

        // update castling rights
        if (!kingsideCastlingRightsLost[c]) {
            if (move.moving() == color + KING || (move.moving() == color + ROOK && (color == WHITE ? move.start() == 7 : move.start() == 63))) {
                kingsideCastlingRightsLost[c] = totalHalfmoves;
                zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
            }
        }
        if (!queensideCastlingRightsLost[c]) {
            if (move.moving() == color + KING || (move.moving() == color + ROOK && (color == WHITE ? move.start() == 0 : move.start() == 56))) {
                queensideCastlingRightsLost[c] = totalHalfmoves;
                zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];
            }
        }
        if (!kingsideCastlingRightsLost[e]) {
            if (color == BLACK ? move.target() == 7 : move.target() == 63) {
                kingsideCastlingRightsLost[e] = totalHalfmoves;
                zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[e];
            }
        }
        if (!queensideCastlingRightsLost[e]) {
            if (color == BLACK ? move.target() == 0 : move.target() == 56) {
                queensideCastlingRightsLost[e] = totalHalfmoves;
                zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[e];
            }
        }

        previousMove.push(move);

        currentLegalMoves = legalMoves();
    }

    // update the board to reverse the inputted move (must have just been move previously played)
    void unmakeMove(Move &move)
    {
        int c = move.moving() >> 3;
        int color = c << 3;
        int e = !color;
        int enemy = e << 3;

        // UNDO PEICE DATA / ZOBRIST HASH
        // Undo zobrist hash for turn change
        zobrist ^= ZOBRIST_TURN_KEY;

        // Undo peice array for moving peice
        peices[move.start()] = move.moving();
        peiceSprites[move.start()].setTexture(peiceTextures[move.moving()]);
        zobrist ^= ZOBRIST_PEICE_KEYS[c][move.moving() % (1 << 3) - 1][move.start()];

        if (move.promotion()) {
            zobrist ^= ZOBRIST_PEICE_KEYS[c][move.promotion() - 1][move.target()];
            
        } else {
            zobrist ^= ZOBRIST_PEICE_KEYS[c][move.moving() % (1 << 3) - 1][move.target()];
        }
        
        if (move.isEnPassant()) {
            peices[move.target()] = 0;
            peiceSprites[move.target()].setTexture(peiceTextures[0]);

            peices[move.target() - 8 + 16 * c] = move.captured();
            peiceSprites[move.target() - 8 + 16 * c].setTexture(peiceTextures[move.captured()]);
        } else {
            peices[move.target()] = move.captured();
            peiceSprites[move.target()].setTexture(peiceTextures[move.captured()]);
        }

        // Undo zobrist hash and peice indices set for capture
        if (move.captured()){
            int captureSquare = move.isEnPassant() ? move.target() - 8 + 16 * c : move.target();
            zobrist ^= ZOBRIST_PEICE_KEYS[e][move.captured() % (1 << 3) - 1][captureSquare];
        }

        // Undo rooks for castling
        if (move.isCastling()) {
            int rookStart;
            int rookEnd;

            int castlingRank = move.target() & 0b11111000;
            if (move.target() % 8 < 4) {
                // Queenside castling
                rookStart = castlingRank;
                rookEnd = castlingRank + 3;
            } else {
                // Kingside castling
                rookStart = castlingRank + 7;
                rookEnd = castlingRank + 5;
            }

            peices[rookStart] = peices[rookEnd];
            peiceSprites[rookStart].setTexture(peiceTextures[peices[rookEnd]]);
            
            peices[rookEnd] = 0;
            peiceSprites[rookEnd].setTexture(peiceTextures[0]);
            
            zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][rookStart];
            zobrist ^= ZOBRIST_PEICE_KEYS[c][ROOK - 1][rookEnd];
        }

        // UBDO BOARD FLAGS
        // Undo king index
        if (move.moving() % (1 << 3) == KING) {
            kingIndex[c] = move.start();
        }

        // undo castling rights
        if (kingsideCastlingRightsLost[c] == totalHalfmoves) {
            kingsideCastlingRightsLost[c] = 0;
            zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
        }
        if (queensideCastlingRightsLost[c] == totalHalfmoves) {
            queensideCastlingRightsLost[c] = 0;
            zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];
        }
        if (kingsideCastlingRightsLost[e] == totalHalfmoves) {
            kingsideCastlingRightsLost[e] = 0;
            zobrist ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[e];
        }
        if (queensideCastlingRightsLost[e] == totalHalfmoves) {
            queensideCastlingRightsLost[e] = 0;
            zobrist ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[e];
        }

        // decrement counters
        --totalHalfmoves;
        if (move.captured() || move.moving() == color + PAWN) {
            halfmovesSincePawnMoveOrCapture.pop();
        } else {
            int prev = halfmovesSincePawnMoveOrCapture.top();
            halfmovesSincePawnMoveOrCapture.pop();
            halfmovesSincePawnMoveOrCapture.push(prev -  1);
        }

        // Undo en passant file
        eligibleEnPassantSquare.pop();

        // Undo position history
        positionHistory.pop_front();
        if (positionHistory.front() != zobrist) {
            throw std::runtime_error("Poopiepeepee");
        }

        // Undo previous move
        previousMove.pop();

        currentLegalMoves = legalMoves();
    }

    // returns true if the last move has put the game into a forced draw (threefold repitition / 50 move rule / insufficient material)
    bool isDraw() const
    {
        return isDrawByFiftyMoveRule() || isDrawByInsufficientMaterial() || isDrawByThreefoldRepitition();
    }

    // returns true if the last move played has led to a draw by threefold repitition
    bool isDrawByThreefoldRepitition() const noexcept
    {
        if (halfmovesSincePawnMoveOrCapture.top() < 8) {
            return false;
        }

        int numPossibleRepitions = halfmovesSincePawnMoveOrCapture.top() / 2 - 1;
        bool repititionFound;
        
        auto it = positionHistory.begin();
        ++it;

        for (int i = 0; i < numPossibleRepitions; ++i) {
            ++it;
            ++it;
            if (*it == zobrist) {
                if (repititionFound) {
                    return true;
                }
                repititionFound = true;
            }
        }
        return false;
    }
    
    // returns true if the last move played has led to a draw by the fifty move rule
    bool isDrawByFiftyMoveRule() const noexcept
    {
        return halfmovesSincePawnMoveOrCapture.top() >= 50;
    }
    
    // returns true if there isnt enough material on the board to deliver checkmate
    bool isDrawByInsufficientMaterial() const noexcept
    {
        int numTotalPeices[2] = {0};
        int numPeices[15] = {0};
        for (int peice : peices) {
            if (peice) {
                ++numTotalPeices[peice >> 3];
                ++numPeices[peice];
            }
            if (numTotalPeices[0] > 3 || numTotalPeices[1] > 3) {
                return false;
            }
        }
        
        if (numTotalPeices[0] == 3 || numTotalPeices[1] == 3) {
            return (numPeices[WHITE + KNIGHT] == 2 || numPeices[BLACK + KNIGHT] == 2) && (numTotalPeices[0] == 1 || numTotalPeices[1] == 1);
        }
        return !(numPeices[WHITE + PAWN] || numPeices[BLACK + PAWN] || numPeices[WHITE + ROOK] || numPeices[BLACK + ROOK] || numPeices[WHITE + QUEEN] || numPeices[BLACK + QUEEN]);
    }
 
    // return true if the player who is to move is currently in check
    bool inCheck() const
    {
        return inCheck(totalHalfmoves % 2);
    }
    
    // return true if the king belonging to the inputted color is currently being attacked
    bool inCheck(int c) const
    {
        int color = c << 3;
        int e = !c;
        int enemy = e << 3;
        int king = kingIndex[c];

        // Pawn checks
        int kingFile = king % 8;
        int ahead = king + 8 - 16 * c;
        if (kingFile != 0 && peices[ahead - 1] == enemy + PAWN) {
            return true;
        }
        if (kingFile != 7 && peices[ahead + 1] == enemy + PAWN) {
            return true;
        }

        // Knight checks
        for (int j = 1; j < KNIGHT_MOVES[king][0]; ++j) {
            if (peices[KNIGHT_MOVES[king][j]] == enemy + KNIGHT) {
                return true;
            }
        }

        // sliding peice checks
        for (int j = king - 8; j >= DIRECTION_BOUNDS[king][B]; j -= 8) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }
        
        for (int j = king + 8; j <= DIRECTION_BOUNDS[king][F]; j += 8) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (int j = king - 1; j >= DIRECTION_BOUNDS[king][L]; j -= 1) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (int j = king + 1; j <= DIRECTION_BOUNDS[king][R]; j += 1) {
            if (peices[j]) {
                if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (int j = king - 9; j >= DIRECTION_BOUNDS[king][BL]; j -= 9) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (int j = king + 9; j <= DIRECTION_BOUNDS[king][FR]; j += 9) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (int j = king - 7; j >= DIRECTION_BOUNDS[king][BR]; j -= 7) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        for (int j = king + 7; j <= DIRECTION_BOUNDS[king][FL]; j += 7) {
            if (peices[j]) {
                if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                    return true;
                }
                break;
            }
        }

        // King checks (seems wierd, but needed for detecting illegal moves)
        for (int j = 1; j < KING_MOVES[king][0]; ++j) {
            if (peices[KING_MOVES[king][j]] == enemy + KING) {
                return true;
            }
        }

        return false;
    }

    // return a string representation of the position in Forsyth–Edwards Notation
    std::string asFEN()
    {
        std::string fen = "";
        int c = totalHalfmoves % 2;

        // Peice placement data
        char pcs[6] = {'P', 'N', 'B', 'R', 'Q', 'K'};
        int gap = 0;
        for (int i = 56; i >= 0; i -= 8) {
            for (int j = 0; j < 8; ++j) {
                if (!peices[i + j]) {
                    ++gap;
                    continue;
                }
                // Add gap charecter if needed
                if (gap) {
                    fen += '0' + gap;
                    gap = 0;
                }
                // Add peice charecter
                fen += pcs[(peices[i + j] % (1 << 3)) - 1] + 32 * (peices[i + j] >> 3);
            }
            // Add gap charecter if needed
            if (gap) {
                fen += '0' + gap;
                gap = 0;
            }
            // Add rank seperator
            if (i != 0) {
                fen += '/';
            }
        }

        // Player to move
        fen += (c) ? " b " : " w ";

        // Castling availiability
        std::string castlingAvailability = "";
        if (!kingsideCastlingRightsLost[0]) {
            castlingAvailability += 'K';
        }
        if (!queensideCastlingRightsLost[0]) {
            castlingAvailability += 'Q';
        }
        if (!kingsideCastlingRightsLost[1]) {
            castlingAvailability += 'k';
        }
        if (!queensideCastlingRightsLost[1]) {
            castlingAvailability += 'q';
        }
        if (castlingAvailability.size() == 0) {
            fen += "- ";
        } else {
            fen += castlingAvailability + " ";
        }

        // En passant target
        if (eligibleEnPassantSquare.top() >= 0) {
            fen += boardIndexToAlgebraicNotation(eligibleEnPassantSquare.top()) + " ";
        } else {
            fen += "- ";
        }

        // Halfmoves since pawn move or capture
        fen += std::to_string(halfmovesSincePawnMoveOrCapture.top());
        fen += ' ';

        // Total moves
        fen += '1' + totalHalfmoves / 2;

        return fen;
    }

    // return true if inputted pseudo legal move is legal in the current position
    bool isLegal(Move &move)
    {
        int c = move.moving() >> 3;
        int color = c << 3;
        int e = !color;
        int enemy = e << 3;

        // Seperately check legality of castling moves
        if (move.isCastling()) {
            return castlingMoveIsLegal(move);
        }
        
        // Update peices array first to check legality
        peices[move.start()] = 0;
        peices[move.target()] = move.promotion() ? color + move.promotion() : move.moving();
        if (move.isEnPassant()) {
            peices[move.target() - 8 + 16 * c] = 0;
        }
        // Update king index
        if (move.moving() % (1 << 3) == KING) {
            kingIndex[c] = move.target();
        }

        // Check if move was illegal
        bool legal = !inCheck(c);

        // Undo change
        peices[move.start()] = move.moving();
        peices[move.target()] = move.captured();
        if (move.isEnPassant()) {
            peices[move.target()] = 0;
            peices[move.target() - 8 + 16 * c] = move.captured();
        }
        // Undo king index
        if (move.moving() % (1 << 3) == KING) {
            kingIndex[c] = move.start();
        }

        return legal;
    }

    // @param move pseudo legal castling move (castling rights are not lost and king is not in check)
    // @return true if the castling move is legal in the current position
    bool castlingMoveIsLegal(Move &move) {
        if (inCheck()) {
            return false;
        }

        int c = totalHalfmoves % 2;
        int color = c << 3;
        int e = !color;
        int enemy = e << 3;
        int castlingRank = move.start() & 0b11111000;

        // Check if anything is attacking squares on king's path
        int s;
        int end;
        if (move.target() - castlingRank < 4) {
            s = castlingRank + 2;
            end = castlingRank + 3;
        } else {
            s = castlingRank + 5;
            end = castlingRank + 6;
        }
        for (; s <= end; ++s) {
            // Pawn attacks
            int file = s % 8;
            int ahead = s + 8 - 16 * c;
            if (file != 0 && peices[ahead - 1] == enemy + PAWN) {
                return false;
            }
            if (file != 7 && peices[ahead + 1] == enemy + PAWN) {
                return false;
            }

            // Knight attacks
            for (int j = 1; j < KNIGHT_MOVES[s][0]; ++j) {
                if (peices[KNIGHT_MOVES[s][j]] == enemy + KNIGHT) {
                    return false;
                }
            }

            // sliding peice attacks
            if (color == BLACK) {
                for (int j = s - 8; j >= DIRECTION_BOUNDS[s][B]; j -= 8) {
                    if (peices[j]) {
                        if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }
                
                for (int j = s - 9; j >= DIRECTION_BOUNDS[s][BL]; j -= 9) {
                    if (peices[j]) {
                        if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }

                for (int j = s - 7; j >= DIRECTION_BOUNDS[s][BR]; j -= 7) {
                    if (peices[j]) {
                        if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }
            
            } else {
                for (int j = s + 8; j <= DIRECTION_BOUNDS[s][F]; j += 8) {
                    if (peices[j]) {
                        if (peices[j] == enemy + ROOK || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }

                for (int j = s + 9; j <= DIRECTION_BOUNDS[s][FR]; j += 9) {
                    if (peices[j]) {
                        if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }

                for (int j = s + 7; j <= DIRECTION_BOUNDS[s][FL]; j += 7) {
                    if (peices[j]) {
                        if (peices[j] == enemy + BISHOP || peices[j] == enemy + QUEEN) {
                            return false;
                        }
                        break;
                    }
                }
            }

            // King attacks
            for (int j = 1; j < KING_MOVES[s][0]; ++j) {
                if (peices[KING_MOVES[s][j]] == enemy + KING) {
                    return false;
                }
            }
        }

        return true;
    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const
    {
        // Draw checkerboard
        for (const sf::RectangleShape &square : squareSprites) {
            target.draw(square);
        }

        // Draw peices
        for (const sf::Sprite &peice : peiceSprites) {
            target.draw(peice);
        }

        target.draw(hoveringPeice);
    }

    void resetSquareHighlights()
    {
        // Color squares default color
        for (int i = 0; i < 64; ++i) {
            squareSprites[i].setFillColor(((i + (i / 8) % 2) % 2 == 0) ? LIGHT_SQUARE_COLOR : DARK_SQUARE_COLOR);
        }

        // Add highlights where needed
        if (!previousMove.empty()) {
            int s = previousMove.top().start();
            int t = previousMove.top().target();
            squareSprites[s].setFillColor(((s + (s / 8) % 2) % 2 == 0) ? LIGHT_PREVIOUS_MOVE : DARK_PREVIOUS_MOVE);
            squareSprites[t].setFillColor(((t + (t / 8) % 2) % 2 == 0) ? LIGHT_PREVIOUS_MOVE : DARK_PREVIOUS_MOVE);
        }

        if (currentlySelected.has_value()) {
            int s = currentlySelected.value();
            squareSprites[s].setFillColor(((s + (s / 8) % 2) % 2 == 0) ? LIGHT_CURRENTLY_SELECTED : DARK_CURRENTLY_SELECTED);

            for (Move &move : currentLegalMoves) {
                if (move.start() == s) {
                    int t = move.target();
                    squareSprites[t].setFillColor(((t + (t / 8) % 2) % 2 == 0) ? LIGHT_AVAILABLE_TARGET : DARK_AVAILABLE_TARGET);
                }
            }
        }
    }

    /**
     * @param algebraic notation for position on chess board (ex e3, a1, c8)
     * @return uint8 index [0, 63] -> [a1, h8] of square on board
     */
    static int algebraicNotationToBoardIndex(const std::string &algebraic)
    {
        if (algebraic.size() != 2) {
            throw std::invalid_argument("Algebraic notation should only be two letters long!");
        }

        int file = algebraic[0] - 'a';
        int rank = algebraic[1] - '1';

        if (file < 0 || file > 7 || rank < 0 || rank > 7) {
            throw std::invalid_argument("Algebraic notation should be in the form [a-h][1-8]!");
        }

        return rank * 8 + file;
    }

    /**
     * @param boardIndex index [0, 63] -> [a1, h8] of square on board
     * @return std::string notation for position on chess board (ex e3, a1, c8)
     */
    static std::string boardIndexToAlgebraicNotation(int boardIndex)
    {
        if (boardIndex < 0 || boardIndex > 63) {
            throw std::invalid_argument("Algebraic notation should only be two letters long!");
        }
        
        char file = 'a' + boardIndex % 8;
        char rank = '1' + boardIndex / 8;

        std::string algebraic;
        algebraic = file;
        algebraic += rank;
        
        return algebraic;
    }
};