#pragma once
#define ui64 uint64_t
#define ui8 uint8_t
#define i16 int16_t
#include "move.h"
#include "position.h"


ui64 getBishopAttacks(const ui8 sq, const ui64 occ);
ui64 getRookAttacks(const ui8 sq, const ui64 occ);
ui64 getQueenAttacks(const ui8 sq, const ui64 occ);

struct tapered_score {
    int soon = 0;
    int late = 0;
    inline tapered_score operator+ (const tapered_score &s) const {
        return {soon + s.soon, late + s.late};
    }

    inline tapered_score operator- (const tapered_score &s) const {
        return {soon - s.soon, late - s.late};
    }

    inline tapered_score &operator+= (const tapered_score &s) {
        soon += s.soon;
        late += s.late;
        return *this;
    }

    inline tapered_score &operator-= (const tapered_score &s) {
        soon -= s.soon;
        late -= s.late;
        return *this;
    }

    inline tapered_score operator* (const int temp) {
        return {soon * temp, late * temp};
    }
};

#define tS(soon, late) tapered_score{soon, late}

int classicEval(const position &pos);
inline bool willItDraw(const position &pos, const ui64 wPieces, const ui64 bPieces);
inline int linearTaper(const int soonVal, const int lateVal, const float phase) {
    return static_cast<int>((1.f - phase) * soonVal + phase*lateVal);
}
inline int linearTaper(const tapered_score &tap, const float phase) {
    return static_cast<int>((1.f - phase)*tap.soon + phase * tap.late);
}

struct evalFeatures {
    static constexpr int weight = 874;
    tapered_score weights[weight];

    static constexpr int danger[8] = {0, 50, 70, 80, 90, 95, 98, 100};
    static constexpr int pieces[7] = {0, 1, 2, 2, 3, 5, 4};

    constexpr int indPieceMat(const ui8 pieceTyp) const { return pieceTyp - 1; }
    constexpr int indPSQT(const ui8 pieceTyp, const uint8_t sq) const { return 6 + (pieceTyp - 1) * 64 + sq; }
    constexpr int indKnightMob(const ui8 mob) const { return 390 + mob; }
    constexpr int indBishopMob(const ui8 mob) const { return 399 + mob; }
    constexpr int indRookMob(const ui8 mob) const { return 413 + mob; }
    constexpr int indQueenMob(const ui8 mob) const { return 428 + mob; }
    constexpr int indKingDanger(const ui8 danger) const { return 455 + danger; }
    constexpr int indPassedPawn(const ui8 sq) const { return 481 + sq; }
    constexpr int indBlockedPasser(const ui8 rank) const { return 545 + rank; }
    constexpr int indIsolatePawn(const ui8 file) const { return 553 + file; }
    
    const int indDoubledPawn = 561;
    const int indTripledPawn = 562;

    constexpr int indPawnSupportPawn(const ui8 rank) const { return 563 + rank; };
    constexpr int indPawnPhalanx(const ui8 rank) const { return 571 + rank; };
    constexpr int indPawnThreats(const ui8 attackedPiece) const { return 578 + attackedPiece; };
    constexpr int indKnightThreats(const ui8 attackedPiece) const { return 584 + attackedPiece; };
    constexpr int indBishopThreats(const ui8 attackedPiece) const { return 590 + attackedPiece; };
    constexpr int indRookThreats(const ui8 attackedPiece) const { return 596 + attackedPiece; };
    constexpr int indQueenThreats(const ui8 attackedPiece) const { return 602 + attackedPiece; };
    constexpr int indKingThreats(const ui8 attackedPiece) const { return 608 + attackedPiece; };

    const int indBishopPair = 615;
    const int indKnightOutpost = 616;
    const int indTempoBonus = 617;

    constexpr int indRookOnOpenFile(const ui8 squ) const {
        return squ + 618;
    };

    constexpr int indRookOnSemiOpenFile(const ui8 squ) const {
        return squ + 682;
    };

    constexpr int indKingOnOpenFile(const ui8 squ) const {
        return squ + 746;
    };

    constexpr int indKingOnSemiOpenFile(const ui8 squ) const {
        return squ + 810;
    };

    inline const tapered_score &getMat(const ui8 pieceTyp) const {
        return weights[indPieceMat(pieceTyp)];
    }

    inline const tapered_score &getPQST(const ui8 pieceTyp, const ui8 squ) const {
        return weights[indPSQT(pieceTyp, squ)];
    }

    inline const tapered_score &getKnightMob(const ui8 mob) const {
        return weights[indKnightMob(mob)];
    }

    inline const tapered_score &getBishopMob(const ui8 mob) const {
        return weights[indBishopMob(mob)];
    }

    inline const tapered_score &getRookMob(const ui8 mob) const {
        return weights[indRookMob(mob)];
    }

    inline const tapered_score &getQueenMob(const ui8 mob) const {
        return weights[indQueenMob(mob)];
    }

    inline const tapered_score &getKingDanger(const ui8 danger) const {
        return weights[indKingDanger(danger)];
    }

    inline const tapered_score &getPassedPawnEval(const ui8 squ) const {
        return weights[indPassedPawn(squ)];
    }

    inline const tapered_score &getBlockedPasserEval(const ui8 rank) const {
        return weights[indBlockedPasser(rank)];
    }

    inline const tapered_score &getIsolatedPawnEval(const ui8 file) const {
        return weights[indIsolatePawn(file)];
    }

    inline const tapered_score &getDoubledPawn() const {
        return weights[indDoubledPawn];
    }

    inline const tapered_score &getTripledPawnEval() const {
        return weights[indTripledPawn];
    }

    inline const tapered_score &getPawnSupportingPawn(const ui8 rank) const {
        return weights[indPawnSupportPawn(rank)];
    }

    inline const tapered_score &getPawnPhalanx(const ui8 rank) const {
        return weights[indPawnPhalanx(rank)];
    }

    inline const tapered_score &getPawnThreat(const ui8 attackedPiece) const {
        return weights[indPawnThreats(attackedPiece)];
    }

    inline const tapered_score &getKnightThreat(const ui8 attackedPiece) const {
        return weights[indKnightThreats(attackedPiece)];
    }

    inline const tapered_score &getBishopThreat(const ui8 attackedPiece) const {
        return weights[indBishopThreats(attackedPiece)];
    }

    inline const tapered_score &getRookThreat(const ui8 attackedPiece) const {
        return weights[indRookThreats(attackedPiece)];
    }

    inline const tapered_score &getQueenThreat(const ui8 attackedPiece) const {
        return weights[indQueenThreats(attackedPiece)];
    }

    inline const tapered_score &getKingThreat(const ui8 attackedPiece) const {
        return weights[indKingThreats(attackedPiece)];
    }

    inline const tapered_score &getBishopPairEval() const {
        return weights[indBishopPair];
    }

    inline const tapered_score &getKnightOutputEval() const {
        return weights[indKnightOutpost];
    }

    inline const tapered_score &getTempoBonus() const {
        return weights[indTempoBonus];
    }

    inline const tapered_score &getRookOnOpenFileBonus(const ui8 squ) const {
        return weights[indRookOnOpenFile(squ)];
    }

    inline const tapered_score &getRookOnSemiOpenFileBonus(const ui8 squ) const {
        return weights[indRookOnSemiOpenFile(squ)];
    }

    inline const tapered_score &getKingOnOpenFileEval(const ui8 squ) const {
        return weights[indKingOnOpenFile(squ)];
    }

    inline const tapered_score &getKingOnSemiOpenFileEval(const ui8 squ) const {
        return weights[indKingOnSemiOpenFile(squ)];
    }
};

constexpr evalFeatures weights = {{
	// These are the weights used in the hand-crafted evaluation
	// It is a bit messy, but life is short and organizing them takes a while
	
	// 1. Material values (pawn, knight, bishop, rook, queen, king)
	tS(80, 92), tS(356, 355), tS(368, 380), tS(479, 684), tS(1107, 1245), tS(0, 0),

	// 2. Piece-square tables
	// Be careful, counter-intuitively the first element corresponds to white's bottom-left corner

	// 2.1 Pawn PSQT
	tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0),
	tS(-18, 29), tS(-18, 40), tS(-9, 30), tS(-5, 32), tS(3, 42), tS(30, 23), tS(38, 19), tS(1, 8),
	tS(-28, 21), tS(-27, 29), tS(-16, 16), tS(-13, 24), tS(0, 23), tS(-2, 17), tS(14, 14), tS(-4, 3),
	tS(-21, 25), tS(-24, 39), tS(-10, 19), tS(2, 16), tS(4, 16), tS(8, 12), tS(1, 22), tS(-2, 7),
	tS(-16, 41), tS(-15, 41), tS(-10, 27), tS(-7, 14), tS(15, 14), tS(19, 10), tS(11, 28), tS(12, 16),
	tS(-5, 64), tS(-32, 85), tS(6, 50), tS(17, 45), tS(21, 40), tS(67, 29), tS(46, 75), tS(32, 59),
	tS(80, 208), tS(99, 200), tS(60, 206), tS(113, 164), tS(61, 169), tS(79, 162), tS(-1, 212), tS(2, 211),
	tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0),
	
	// 2.2 Knight PSQT
	tS(-82, -21), tS(-40, -16), tS(-41, -5), tS(-23, -1), tS(-20, 4), tS(-14, -8), tS(-35, -10), tS(-58, -17),
	tS(-49, -20), tS(-39, 2), tS(-21, 4), tS(-5, 8), tS(-5, 7), tS(-8, 4), tS(-19, -4), tS(-22, 9),
	tS(-42, -6), tS(-19, 6), tS(-4, 14), tS(4, 28), tS(18, 25), tS(6, 8), tS(2, 3), tS(-16, -3),
	tS(-21, 15), tS(-6, 8), tS(7, 27), tS(14, 26), tS(22, 30), tS(19, 14), tS(28, 5), tS(-5, 8),
	tS(-7, 5), tS(7, 16), tS(24, 25), tS(48, 26), tS(32, 19), tS(50, 19), tS(20, 13), tS(29, -7),
	tS(-17, -4), tS(9, 3), tS(39, 21), tS(50, 12), tS(59, 7), tS(86, -7), tS(30, -7), tS(13, -18),
	tS(-47, -11), tS(-19, -4), tS(9, -9), tS(11, 2), tS(5, -11), tS(44, -18), tS(11, -18), tS(0, -35),
	tS(-130, -84), tS(-110, -22), tS(-59, -3), tS(-38, -15), tS(-20, -8), tS(-84, -30), tS(-96, -19), tS(-114, -83),

	// 2.3 Bishop PSQT
	tS(-8, -17), tS(20, -6), tS(-4, -5), tS(-10, -5), tS(-3, -7), tS(-10, 6), tS(12, -16), tS(12, -42),
	tS(8, -13), tS(7, -15), tS(18, -20), tS(-1, 0), tS(11, -4), tS(20, -11), tS(27, -7), tS(13, -26),
	tS(-12, -7), tS(13, 1), tS(9, 5), tS(10, 8), tS(14, 14), tS(12, 6), tS(13, -4), tS(10, -16),
	tS(-12, -8), tS(-13, 5), tS(4, 10), tS(28, 7), tS(27, 4), tS(4, 6), tS(-4, 3), tS(0, -20),
	tS(-21, 2), tS(-4, 6), tS(8, 4), tS(35, 14), tS(27, 4), tS(18, 7), tS(-4, 4), tS(-10, -5),
	tS(-8, 3), tS(14, -5), tS(10, 5), tS(21, -12), tS(17, -6), tS(59, -3), tS(37, -8), tS(23, -4),
	tS(-26, -15), tS(-4, -9), tS(-14, -4), tS(-17, -4), tS(-4, -18), tS(-7, -10), tS(0, -15), tS(-21, -17),
	tS(-24, -2), tS(-54, 6), tS(-29, -10), tS(-78, 3), tS(-74, -3), tS(-74, -8), tS(-44, -8), tS(-59, -16),

	// 2.4 Rook PSQT
	tS(-18, 13), tS(-12, 6), tS(-5, 6), tS(7, 0), tS(10, -5), tS(7, 3), tS(-3, -14), tS(-18, -10),
	tS(-33, 3), tS(-27, 14), tS(-15, 5), tS(-11, 2), tS(-5, -8), tS(3, -10), tS(15, -17), tS(-27, -18),
	tS(-36, 13), tS(-21, 18), tS(-22, 12), tS(-14, 7), tS(-12, 1), tS(-1, -5), tS(29, -22), tS(5, -27),
	tS(-36, 22), tS(-12, 30), tS(-11, 20), tS(-8, 14), tS(-7, 6), tS(-9, 9), tS(11, 1), tS(-12, -6),
	tS(-19, 23), tS(3, 17), tS(-6, 30), tS(2, 18), tS(1, 3), tS(19, 1), tS(22, 6), tS(12, -8),
	tS(-25, 20), tS(21, 22), tS(13, 19), tS(7, 13), tS(40, -5), tS(50, -4), tS(94, -7), tS(49, -12),
	tS(-18, 18), tS(-3, 33), tS(12, 35), tS(18, 23), tS(5, 20), tS(58, 14), tS(44, 12), tS(37, 1),
	tS(-42, 21), tS(4, 22), tS(-5, 37), tS(-19, 34), tS(9, 34), tS(14, 25), tS(47, 20), tS(35, 17),
	
	// 2.5 Queen PSQT
	tS(-14, -19), tS(-7, -16), tS(0, -16), tS(2, 6), tS(4, -9), tS(-7, -19), tS(-1, -22), tS(-10, -24),
	tS(-9, -19), tS(-4, -24), tS(2, -16), tS(7, -2), tS(6, 3), tS(14, -23), tS(12, -35), tS(30, -72),
	tS(-16, -9), tS(-5, 4), tS(-5, 15), tS(-10, 18), tS(-2, 27), tS(4, 21), tS(12, 12), tS(3, 12),
	tS(-12, 3), tS(-12, 13), tS(-9, 17), tS(-4, 36), tS(-6, 35), tS(-5, 38), tS(6, 23), tS(4, 39),
	tS(-20, 22), tS(-8, 15), tS(-11, 22), tS(-14, 39), tS(1, 43), tS(5, 50), tS(16, 49), tS(13, 31),
	tS(-5, 4), tS(-10, 13), tS(-8, 40), tS(-3, 48), tS(14, 50), tS(60, 43), tS(72, 4), tS(51, 36),
	tS(-15, 4), tS(-30, 16), tS(-19, 34), tS(-24, 54), tS(-7, 58), tS(13, 42), tS(-5, 29), tS(49, 23),
	tS(-42, 17), tS(-19, 10), tS(0, 33), tS(-14, 53), tS(11, 37), tS(31, 25), tS(49, -13), tS(-4, 18),

	// 2.6 King PSQT
	tS(38, -99), tS(75, -72), tS(43, -44), tS(-53, -23), tS(3, -53), tS(-31, -28), tS(43, -64), tS(44, -108),
	tS(54, -52), tS(23, -18), tS(5, -7), tS(-31, 9), tS(-38, 14), tS(-17, -1), tS(26, -25), tS(27, -47),
	tS(-44, -27), tS(6, -12), tS(-40, 23), tS(-56, 38), tS(-48, 41), tS(-60, 23), tS(-29, -2), tS(-70, -17),
	tS(-57, -18), tS(-39, 8), tS(-59, 39), tS(-77, 61), tS(-81, 64), tS(-43, 34), tS(-74, 18), tS(-148, 4),
	tS(-65, -10), tS(-24, 24), tS(-30, 57), tS(-79, 74), tS(-55, 57), tS(-52, 45), tS(-44, 26), tS(-128, 11),
	tS(-98, -2), tS(49, 24), tS(-1, 66), tS(9, 66), tS(4, 55), tS(27, 45), tS(12, 33), tS(-31, -4),
	tS(-50, -9), tS(44, 31), tS(18, 56), tS(32, 41), tS(12, 33), tS(12, 38), tS(34, 31), tS(34, -9),
	tS(11, -98), tS(36, -10), tS(67, -5), tS(-60, 13), tS(-38, -13), tS(29, -17), tS(22, -3), tS(79, -96),
	
	// 3. Piece mobility
	// Score tables depending on the pseudolegal moves available

	// 3.1 Knight mobility (0-8)
	tS(-27, -36), tS(-6, -4), tS(6, 14), tS(10, 24), tS(15, 31), tS(20, 38), tS(26, 38), tS(33, 32), tS(35, 22),

	// 3.2 Bishop mobility (0-13)
	tS(-27, -46), tS(-15, -20), tS(-3, -6), tS(4, 8), tS(12, 16), tS(16, 28), tS(20, 32),
	tS(23, 35), tS(22, 41), tS(26, 36), tS(32, 31), tS(41, 29), tS(39, 42), tS(42, 24),

	// 3.3 Rook mobility (0-14)
	tS(-20, -25), tS(-8, -10), tS(-4, -6), tS(0, 4), tS(0, 9), tS(7, 12), tS(9, 18), tS(13, 21),
	tS(17, 24), tS(22, 28), tS(25, 29), tS(24, 34), tS(32, 34), tS(43, 29), tS(45, 30),

	// 3.4 Queen mobility (0-27)
	tS(-6, -97), tS(-5, -123), tS(-13, -94), tS(-10, -69), tS(-8, -55), tS(-5, -42), tS(-1, -26),
	tS(1, -16), tS(2, 0), tS(6, -1), tS(7, 13), tS(10, 18), tS(11, 25), tS(12, 29),
	tS(11, 38), tS(14, 46), tS(11, 60), tS(9, 69), tS(18, 69), tS(36, 62), tS(23, 81),
	tS(72, 58), tS(61, 70), tS(72, 63), tS(128, 70), tS(100, 49), tS(98, 71), tS(92, 60),

	// 4. King safety (1-25 danger points)
	// Danger points are given for attacks near the king, and then scaled according to the attacker count

	tS(-5, -5), tS(-5, -5), tS(-20, -20), tS(-25, -25), tS(-36, -36),
	tS(-56, -56), tS(-72, -72), tS(-90, -90), tS(-133, -133), tS(-190, -190),
	tS(-222, -222), tS(-252, -252), tS(-255, -255), tS(-178, -178), tS(-322, -322),
	tS(-332, -332), tS(-350, -350), tS(-370, -370), tS(-400, -400), tS(-422, -422),
	tS(-425, -425), tS(-430, -430), tS(-435, -435), tS(-440, -440), tS(-445, -445),

	// 5. Pawn structure
	// Collection of features to evaluate the pawn structure

	// 5.1 Passed pawn bonus PSQT
	tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0),
	tS(-9, 8), tS(-3, 12), tS(-15, 13), tS(-13, 11), tS(4, -7), tS(2, 5), tS(12, 12), tS(5, 8),
	tS(-3, 14), tS(-16, 24), tS(-21, 22), tS(-19, 15), tS(-18, 16), tS(-12, 17), tS(-19, 41), tS(17, 11),
	tS(6, 55), tS(1, 48), tS(-11, 39), tS(-5, 33), tS(-17, 39), tS(-4, 42), tS(-14, 60), tS(-3, 51),
	tS(29, 93), tS(24, 93), tS(31, 67), tS(21, 65), tS(3, 63), tS(15, 72), tS(-12, 93), tS(-10, 94),
	tS(53, 162), tS(79, 152), tS(47, 139), tS(27, 117), tS(26, 115), tS(16, 134), tS(-6, 136), tS(-6, 150),
	tS(57, 89), tS(27, 88), tS(43, 87), tS(36, 82), tS(45, 79), tS(35, 87), tS(49, 85), tS(48, 82),
	tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0), tS(0, 0),

	// 5.2 Blocked passed pawn penalties by rank
	tS(0, 0), tS(-25, 13), tS(-14, 10), tS(-15, -15), tS(-6, -37), tS(11, -110), tS(-31, -126), tS(0, 0),

	// 5.3 Isolated pawns by file
	tS(-9, 2), tS(-6, -12), tS(-12, -7), tS(-11, -15), tS(-12, -13), tS(-13, -6), tS(-1, -14), tS(-5, 0),

	// 5.4 Doubled and tripled pawns
	tS(-6, -23), tS(4, -61),

	// 5.5 Supported pawn bonus (by rank)
	tS(0, 0), tS(0, 0), tS(18, 12), tS(13, 7), tS(16, 15), tS(39, 35), tS(150, 30), tS(0, 0),

	// 5.6 Pawn phalanx (by rank)
	tS(0, 0), tS(7, -4), tS(15, 7), tS(26, 17), tS(59, 54), tS(122, 122), tS(122, 122), tS(0, 0),

	// 6. Threat matrix
	// It somewhat helps to see beyond the horizon, also it ruins nps
	// Threatened king values are 0 due to the tuning dataset lacking such positions

	// Pawn attacking
	tS(0, 0), tS(63, -1), tS(63, 34), tS(80, -13), tS(62, -19), tS(0, 0),
	// Knight attacking
	tS(-11, 4), tS(0, 0), tS(33, 19), tS(62, -6), tS(44, -40), tS(0, 0),
	// Bishop attacking
	tS(-4, 8), tS(17, 23), tS(0, 0), tS(44, 3), tS(64, 58), tS(0, 0),
	// Rook attacking
	tS(-7, 5), tS(5, 9), tS(15, 4), tS(0, 0), tS(65, 3), tS(0, 0),
	// Queen attacking
	tS(-6, 11), tS(0, 4), tS(-1, 22), tS(-1, 5), tS(0, 0), tS(0, 0),
	// King attacking
	tS(23, 35), tS(-40, 17), tS(-30, 28), tS(-40, 11), tS(0, 0), tS(0, 0),

	// 7. Misc & piece-specific evaluation
	// Features that don't really fit into other categories

	// 7.1 Bishop pairs
	tS(23, 73),

	// 7.2 Knight outposts
	tS(7, 21),

	// 7.3 Tempo bonus
	tS(20, 0),

	// 7.4 Rook open file bonus PSQT
	tS(30, 8), tS(24, 26), tS(20, 32), tS(19, 20), tS(31, 16), tS(43, 4), tS(81, 18), tS(93, 24),
	tS(40, 5), tS(23, 6), tS(31, 16), tS(24, 16), tS(36, 18), tS(34, 14), tS(38, 9), tS(77, 11),
	tS(29, 9), tS(11, 7), tS(21, 10), tS(13, 18), tS(33, 17), tS(31, 8), tS(31, 14), tS(71, 14),
	tS(36, 1), tS(0, 0), tS(5, 12), tS(12, 12), tS(21, 15), tS(24, 8), tS(47, -6), tS(85, 7),
	tS(27, 4), tS(21, 2), tS(28, -1), tS(21, 8), tS(29, 10), tS(24, -2), tS(47, -8), tS(71, 6),
	tS(30, 4), tS(9, -5), tS(19, 3), tS(25, 10), tS(25, 13), tS(14, 11), tS(27, -2), tS(42, 5),
	tS(36, 5), tS(17, 1), tS(17, 2), tS(37, 0), tS(33, 8), tS(-6, 6), tS(8, 3), tS(35, 8),
	tS(65, 7), tS(16, 10), tS(9, 10), tS(13, 12), tS(1, 0), tS(21, 4), tS(-9, 4), tS(35, -4),
	
	// 7.5 Rook semi-open file bonus PSQT
	tS(15, 7), tS(15, 1), tS(16, 5), tS(17, 2), tS(15, 3), tS(17, -7), tS(51, -5), tS(35, -1),
	tS(21, -3), tS(22, -18), tS(18, -3), tS(33, -21), tS(23, -10), tS(15, 1), tS(27, -16), tS(37, -9),
	tS(17, 6), tS(6, -13), tS(17, -6), tS(22, -16), tS(17, -8), tS(4, -2), tS(8, -4), tS(4, 17),
	tS(16, 19), tS(-14, -9), tS(9, -5), tS(14, 1), tS(17, -1), tS(-7, 6), tS(11, 3), tS(13, 10),
	tS(5, 49), tS(3, 28), tS(9, 17), tS(29, 16), tS(31, -1), tS(13, 9), tS(17, 10), tS(23, 33),
	tS(30, 65), tS(28, 31), tS(20, 27), tS(45, 4), tS(39, 22), tS(38, -7), tS(21, 10), tS(-11, 44),
	tS(45, 67), tS(55, 19), tS(30, 24), tS(24, 31), tS(0, 12), tS(20, 9), tS(21, 8), tS(57, 39),
	tS(47, 65), tS(33, 36), tS(-15, 27), tS(26, 19), tS(-37, 21), tS(-6, 1), tS(6, 1), tS(-43, 35),

	// 7.6 King on open file delta PSQT
	tS(-120, -11), tS(-120, -14), tS(-102, -4), tS(-76, 11), tS(-71, 28), tS(-97, 10), tS(-110, 25), tS(-102, 28),
	tS(-120, -25), tS(-120, -4), tS(-89, 2), tS(-88, 4), tS(-102, 10), tS(-102, 12), tS(-112, 17), tS(-46, -2),
	tS(-68, -25), tS(-46, 0), tS(-64, -6), tS(-71, 1), tS(-96, 1), tS(-49, 2), tS(-77, 12), tS(-46, -3),
	tS(-40, -32), tS(-34, -13), tS(-77, -4), tS(-101, 1), tS(-95, -1), tS(-67, 5), tS(-41, 2), tS(14, -5),
	tS(-24, -26), tS(-35, -20), tS(-91, -18), tS(-93, -14), tS(-42, -3), tS(-47, 2), tS(-30, 0), tS(-19, -4),
	tS(11, -32), tS(-47, -31), tS(-57, -45), tS(-96, -25), tS(-40, -14), tS(-43, -12), tS(-41, -8), tS(-18, -6),
	tS(-46, -58), tS(-54, -54), tS(-104, -49), tS(-24, -35), tS(-43, -10), tS(-43, -3), tS(-34, -24), tS(27, -19),
	tS(-23, -35), tS(-35, -86), tS(-44, -53), tS(-53, -28), tS(-44, -6), tS(-35, -9), tS(-41, -46), tS(56, -61),
	
	// 7.7 King on semi-open file delta PSQT
	tS(-54, 116), tS(-102, 74), tS(-49, 28), tS(-20, 25), tS(-16, 19), tS(-34, 18), tS(-33, 28), tS(-49, 67),
	tS(-59, 66), tS(-53, 28), tS(-40, 26), tS(-25, 11), tS(-22, 0), tS(-28, 10), tS(-34, 20), tS(-29, 27),
	tS(-44, 49), tS(-10, 13), tS(-31, 2), tS(-21, -9), tS(-20, -18), tS(-33, 11), tS(-5, 9), tS(-12, 29),
	tS(40, -2), tS(-44, 2), tS(-35, 8), tS(10, -23), tS(-38, -18), tS(-47, 1), tS(-38, 3), tS(-24, 8),
	tS(27, -12), tS(-46, -20), tS(-48, -18), tS(-5, -42), tS(-28, -28), tS(-61, -4), tS(-41, -14), tS(-55, -5),
	tS(-56, -4), tS(-17, -29), tS(-104, -42), tS(-9, -59), tS(-5, -58), tS(-50, -34), tS(-12, -25), tS(-21, -12),
	tS(-6, -15), tS(42, -39), tS(-74, -64), tS(-55, -34), tS(-25, -39), tS(-26, -29), tS(-18, -26), tS(-17, -54),
	tS(12, 55), tS(-14, 28), tS(56, -83), tS(-69, 19), tS(-76, -68), tS(-43, -31), tS(-7, 42), tS(-15, 17),
}};

// Lookup bitboards -------------------------------------------------------------------------------

// Used to select passed pawn candidates
constexpr std::array<ui64, 64> WhitePassedPawnMask = {
	0x0303030303030300, 0x0707070707070700, 0x0e0e0e0e0e0e0e00, 0x1c1c1c1c1c1c1c00, 0x3838383838383800, 0x7070707070707000, 0xe0e0e0e0e0e0e000, 0xc0c0c0c0c0c0c000,
	0x0303030303030000, 0x0707070707070000, 0x0e0e0e0e0e0e0000, 0x1c1c1c1c1c1c0000, 0x3838383838380000, 0x7070707070700000, 0xe0e0e0e0e0e00000, 0xc0c0c0c0c0c00000,
	0x0303030303000000, 0x0707070707000000, 0x0e0e0e0e0e000000, 0x1c1c1c1c1c000000, 0x3838383838000000, 0x7070707070000000, 0xe0e0e0e0e0000000, 0xc0c0c0c0c0000000,
	0x0303030300000000, 0x0707070700000000, 0x0e0e0e0e00000000, 0x1c1c1c1c00000000, 0x3838383800000000, 0x7070707000000000, 0xe0e0e0e000000000, 0xc0c0c0c000000000,
	0x0303030000000000, 0x0707070000000000, 0x0e0e0e0000000000, 0x1c1c1c0000000000, 0x3838380000000000, 0x7070700000000000, 0xe0e0e00000000000, 0xc0c0c00000000000,
	0x0303000000000000, 0x0707000000000000, 0x0e0e000000000000, 0x1c1c000000000000, 0x3838000000000000, 0x7070000000000000, 0xe0e0000000000000, 0xc0c0000000000000,
	0x0300000000000000, 0x0700000000000000, 0x0e00000000000000, 0x1c00000000000000, 0x3800000000000000, 0x7000000000000000, 0xe000000000000000, 0xc000000000000000,
	0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000
};

constexpr std::array<ui64, 64> BlackPassedPawnMask = {
	0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
	0x0000000000000003, 0x0000000000000007, 0x000000000000000e, 0x000000000000001c, 0x0000000000000038, 0x0000000000000070, 0x00000000000000e0, 0x00000000000000c0,
	0x0000000000000303, 0x0000000000000707, 0x0000000000000e0e, 0x0000000000001c1c, 0x0000000000003838, 0x0000000000007070, 0x000000000000e0e0, 0x000000000000c0c0,
	0x0000000000030303, 0x0000000000070707, 0x00000000000e0e0e, 0x00000000001c1c1c, 0x0000000000383838, 0x0000000000707070, 0x0000000000e0e0e0, 0x0000000000c0c0c0,
	0x0000000003030303, 0x0000000007070707, 0x000000000e0e0e0e, 0x000000001c1c1c1c, 0x0000000038383838, 0x0000000070707070, 0x00000000e0e0e0e0, 0x00000000c0c0c0c0,
	0x0000000303030303, 0x0000000707070707, 0x0000000e0e0e0e0e, 0x0000001c1c1c1c1c, 0x0000003838383838, 0x0000007070707070, 0x000000e0e0e0e0e0, 0x000000c0c0c0c0c0,
	0x0000030303030303, 0x0000070707070707, 0x00000e0e0e0e0e0e, 0x00001c1c1c1c1c1c, 0x0000383838383838, 0x0000707070707070, 0x0000e0e0e0e0e0e0, 0x0000c0c0c0c0c0c0,
	0x0003030303030303, 0x0007070707070707, 0x000e0e0e0e0e0e0e, 0x001c1c1c1c1c1c1c, 0x0038383838383838, 0x0070707070707070, 0x00e0e0e0e0e0e0e0, 0x00c0c0c0c0c0c0c0
};

// Filters passed pawn candidates
constexpr std::array<ui64, 64> WhitePassedPawnFilter = {
	0x0101010101010100, 0x0202020202020200, 0x0404040404040400, 0x0808080808080800, 0x1010101010101000, 0x2020202020202000, 0x4040404040404000, 0x8080808080808000,
	0x0101010101010000, 0x0202020202020000, 0x0404040404040000, 0x0808080808080000, 0x1010101010100000, 0x2020202020200000, 0x4040404040400000, 0x8080808080800000,
	0x0101010101000000, 0x0202020202000000, 0x0404040404000000, 0x0808080808000000, 0x1010101010000000, 0x2020202020000000, 0x4040404040000000, 0x8080808080000000,
	0x0101010100000000, 0x0202020200000000, 0x0404040400000000, 0x0808080800000000, 0x1010101000000000, 0x2020202000000000, 0x4040404000000000, 0x8080808000000000,
	0x0101010000000000, 0x0202020000000000, 0x0404040000000000, 0x0808080000000000, 0x1010100000000000, 0x2020200000000000, 0x4040400000000000, 0x8080800000000000,
	0x0101000000000000, 0x0202000000000000, 0x0404000000000000, 0x0808000000000000, 0x1010000000000000, 0x2020000000000000, 0x4040000000000000, 0x8080000000000000,
	0x0100000000000000, 0x0200000000000000, 0x0400000000000000, 0x0800000000000000, 0x1000000000000000, 0x2000000000000000, 0x4000000000000000, 0x8000000000000000,
	0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000
};

constexpr std::array<ui64, 64> BlackPassedPawnFilter = {
	0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
	0x0000000000000001, 0x0000000000000002, 0x0000000000000004, 0x0000000000000008, 0x0000000000000010, 0x0000000000000020, 0x0000000000000040, 0x0000000000000080,
	0x0000000000000101, 0x0000000000000202, 0x0000000000000404, 0x0000000000000808, 0x0000000000001010, 0x0000000000002020, 0x0000000000004040, 0x0000000000008080,
	0x0000000000010101, 0x0000000000020202, 0x0000000000040404, 0x0000000000080808, 0x0000000000101010, 0x0000000000202020, 0x0000000000404040, 0x0000000000808080,
	0x0000000001010101, 0x0000000002020202, 0x0000000004040404, 0x0000000008080808, 0x0000000010101010, 0x0000000020202020, 0x0000000040404040, 0x0000000080808080,
	0x0000000101010101, 0x0000000202020202, 0x0000000404040404, 0x0000000808080808, 0x0000001010101010, 0x0000002020202020, 0x0000004040404040, 0x0000008080808080,
	0x0000010101010101, 0x0000020202020202, 0x0000040404040404, 0x0000080808080808, 0x0000101010101010, 0x0000202020202020, 0x0000404040404040, 0x0000808080808080,
	0x0001010101010101, 0x0002020202020202, 0x0004040404040404, 0x0008080808080808, 0x0010101010101010, 0x0020202020202020, 0x0040404040404040, 0x0080808080808080
};

constexpr std::array<ui64, 64> KingArea = {
	0x0000000000000303, 0x0000000000000707, 0x0000000000000e0e, 0x0000000000001c1c, 0x0000000000003838, 0x0000000000007070, 0x000000000000e0e0, 0x000000000000c0c0,
	0x0000000000030303, 0x0000000000070707, 0x00000000000e0e0e, 0x00000000001c1c1c, 0x0000000000383838, 0x0000000000707070, 0x0000000000e0e0e0, 0x0000000000c0c0c0,
	0x0000000003030300, 0x0000000007070700, 0x000000000e0e0e00, 0x000000001c1c1c00, 0x0000000038383800, 0x0000000070707000, 0x00000000e0e0e000, 0x00000000c0c0c000,
	0x0000000303030000, 0x0000000707070000, 0x0000000e0e0e0000, 0x0000001c1c1c0000, 0x0000003838380000, 0x0000007070700000, 0x000000e0e0e00000, 0x000000c0c0c00000,
	0x0000030303000000, 0x0000070707000000, 0x00000e0e0e000000, 0x00001c1c1c000000, 0x0000383838000000, 0x0000707070000000, 0x0000e0e0e0000000, 0x0000c0c0c0000000,
	0x0003030300000000, 0x0007070700000000, 0x000e0e0e00000000, 0x001c1c1c00000000, 0x0038383800000000, 0x0070707000000000, 0x00e0e0e000000000, 0x00c0c0c000000000,
	0x0303030000000000, 0x0707070000000000, 0x0e0e0e0000000000, 0x1c1c1c0000000000, 0x3838380000000000, 0x7070700000000000, 0xe0e0e00000000000, 0xc0c0c00000000000,
	0x0303000000000000, 0x0707000000000000, 0x0e0e000000000000, 0x1c1c000000000000, 0x3838000000000000, 0x7070000000000000, 0xe0e0000000000000, 0xc0c0000000000000
};

constexpr std::array<ui64, 8> Files = {
	0b0000000100000001000000010000000100000001000000010000000100000001,
	0b0000001000000010000000100000001000000010000000100000001000000010,
	0b0000010000000100000001000000010000000100000001000000010000000100,
	0b0000100000001000000010000000100000001000000010000000100000001000,
	0b0001000000010000000100000001000000010000000100000001000000010000,
	0b0010000000100000001000000010000000100000001000000010000000100000,
	0b0100000001000000010000000100000001000000010000000100000001000000,
	0b1000000010000000100000001000000010000000100000001000000010000000
};

constexpr std::array<ui64, 8> IsolatedPawnMask = {
	0x0202020202020202, 0x0505050505050505, 0x0a0a0a0a0a0a0a0a, 0x1414141414141414, 0x2828282828282828, 0x5050505050505050, 0xa0a0a0a0a0a0a0a0, 0x4040404040404040,
};

constexpr ui64 OutpostFilter = 0b00000000'00000000'00000000'00111100'00111100'00000000'00000000'00000000; // middle 4x2 region
