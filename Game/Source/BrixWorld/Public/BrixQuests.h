#pragma once

#include "CoreMinimal.h"
#include "BrixEconomy.h"
#include "BrixTalk.h"

// Quest lines, daily jobs and reputation (QST-001 to QST-007; ADR-0012): a port of the slice's Econ.Quests (economy.js),
// itself Tools/citysim/quests.py, rule for rule. The quest lines are data (World/rules/quests.json, carried in
// Game/Data/life.json): stages with a goal, the lines said at each in the dialogue's own node format, and endings with
// their rewards, paid through the ledger from sys:rewards (QST-004). Which resident offers which job on which day is
// worked out from (resident, day) and the data, never stored (QST-006). QST001_quests_run_as_the_python_does runs a fixed
// walk through them against Python's results (build_play.py quest_script()).
//
// A career's story lines (QST-010 to QST-015; ADR-0056) run on the same rules: what the player did is kept as facts (Flags),
// needs ask about them, a line with no offer starts when its giver is spoken to, a stage can open on another first line,
// a stage with lines and a place is a scene, and some endings let a line be taken again. QST010_story_lines_run_as_the_
// python_does replays a recorded game of the Baker career and the courting (build_play.py story_run()).

struct BRIXWORLD_API FBrixGoal
{
	FString Kind;                 // talk, go, near, have, bank, money, stall, stall_sold, home
	FString Value;                // a resident id, a lot id, an item
	FVector2D Near = FVector2D::ZeroVector;
	double R = 0, From = 0, To = 1440, Number = 0;
};

struct BRIXWORLD_API FBrixQuestStage
{
	FString Log, Start, Then;
	FBrixGoal Goal;
	TMap<FString, FBrixDialogue::FNode> Nodes;   // the talk at a talk stage, or a scene's lines at a place (QST-012)
	TArray<FString> OnDoneGive, OnDoneTake;
	struct FStartAlt { TArray<FString> Need; FString Node; };
	TArray<FStartAlt> Starts;                    // other first lines: the first whose needs hold (QST-011)
};

struct BRIXWORLD_API FBrixQuestEnding
{
	FString Log;
	int64 Pay = 0, Charge = 0;
	TArray<FString> Give, Take;
	TArray<TPair<FString, int32>> Rep;
	TArray<FString> Flags;                       // facts it leaves with the player (QST-010)
	bool bRetake = false;                        // the line can be taken again (QST-013)
};

struct BRIXWORLD_API FBrixQuestLine
{
	FString Id, Title, Summary, Giver, Start;
	bool bStarter = false;
	TArray<FString> Needs;
	TArray<FString> Need;                        // needs as on a reply: when the line is on offer (QST-010)
	FString Line, Rank;                          // a story line's career (baker, romance) and the rank it gives
	bool Opens() const { return !Giver.IsEmpty() && OfferStart.IsEmpty(); }   // no offer talk: it starts when its giver is spoken to (QST-011)
	TMap<FString, FBrixQuestStage> Stages;
	TMap<FString, FBrixQuestEnding> Endings;
	FString OfferLine, OfferStart;            // how the giver offers it in a talk
	TMap<FString, FBrixDialogue::FNode> OfferNodes;
};

struct BRIXWORLD_API FBrixJobKind
{
	FString Id, Title, Offer, Log;
	TArray<FString> Roles, Needs, Items;
	TMap<FString, FString> RoleNeeds;
	int64 ChanceBp = 0, Pay = 0, PayPer100m = 0;
	int32 Hours = 1, Minutes = 0, Rep = 0, FailRep = 0;
};

struct BRIXWORLD_API FBrixQuestRules
{
	FString First;
	TArray<FBrixQuestLine> Quests;            // in the file's order
	int32 MinRep = -20;
	TArray<FBrixJobKind> Jobs;                // in the file's order: a role's job is the first kind that names it
	TMap<FString, FString> Lines;             // ask, accept, decline, none, busy, hand_over, thanks
	TArray<TArray<FString>> Exclusive;        // groups of facts of which one at most is ever true
	const FBrixQuestLine* Find(const FString& Id) const { return Quests.FindByPredicate([&](const FBrixQuestLine& Q) { return Q.Id == Id; }); }
	const FBrixJobKind* Job(const FString& Id) const { return Jobs.FindByPredicate([&](const FBrixJobKind& J) { return J.Id == Id; }); }
};

struct BRIXWORLD_API FBrixQuestResident { FString Id, Role, Home, Work; };

// A job on offer or taken (quests.py job_offer(), take_job())
struct BRIXWORLD_API FBrixJob
{
	FString Kind, Item, Step;
	int32 Giver = -1, Day = 0, Hours = 0, Minutes = 0, Frm = -1, To = -1;
	int64 Pay = 0;
	double Deadline = 0, Worked = 0;
	TOptional<double> Last;
};

// Reads the quest lines and jobs from Game/Data/life.json (already parsed). The dialogue nodes keep each option's need and act.
BRIXWORLD_API bool LoadBrixQuests(const TSharedPtr<FJsonObject>& Life, FBrixQuestRules& OutRules, TArray<FBrixQuestResident>& OutResidents, FString& OutError);

class BRIXWORLD_API FBrixQuests
{
public:
	FBrixQuests(const FBrixQuestRules& Rules, FBrixEconomy& Economy, const TArray<FBrixQuestResident>& Residents);

	const FBrixQuestRules& Q;
	FBrixEconomy& E;
	const TArray<FBrixQuestResident>& Residents;
	struct FActive { FString Stage; int32 Since = 0; };
	TArray<TPair<FString, FActive>> Active;   // in the order taken (a JS object's keys)
	TArray<TPair<FString, FString>> Done;     // quest -> its ending, in the order finished
	struct FNote { int32 Day = 0; double Minute = 0; FString Text; };
	TArray<FNote> Log;
	TOptional<FBrixJob> Job;
	TArray<FString> JobsDone;                 // "resident index|day": offers taken
	TArray<FString> Flags;                    // what the player did, as facts replies ask about (QST-010); kept sorted
	TArray<FString> Again;                    // quest lines taken a second time (QST-013)
	TMap<FString, TMap<FString, int32>> Awarded;   // quest -> resident -> reputation its replies gave, taken back on a retake
	TFunction<bool()> OwnsHome;               // set by whoever holds the property market (PROP-009)
	TFunction<void(const FString&)> OnNote;
	FString& Why;

	struct FCtx { FString Lot; TOptional<FVector2D> Pos; double Minute = 0; };
	int32 Rep(const FString& Rid) const { return E.Rep.FindRef(Rid); }
	void RepAdd(const FString& Rid, int32 N);
	bool Met(const FString& Need) const;
	const FString* DoneWith(const FString& Qid) const;
	const FActive* ActiveOf(const FString& Qid) const;
	bool NewGame();
	bool Start(const FString& Qid);
	const FBrixQuestStage* Stage(const FString& Qid) const;
	TArray<FString> Offers(const FString& Rid) const;
	TArray<FString> TalkingTo(const FString& Rid) const;
	bool NeedOk(const TArray<FString>& Need) const;
	bool Retakes(const FString& Qid) const;                  // it ended in a way that lets it be taken again
	TArray<FString> Scenes(const FCtx& Ctx) const;    // active lines whose stage is a scene at this place
	FString FirstLine(const FString& Qid) const;             // the node its stage's talk opens on
	TArray<int32> Replies(const FString& Qid, const FString& Nid) const;   // the replies open now, as indexes
	bool Act(const FString& Qid, const TArray<FString>& Acts, const FString& Rid);
	void Check(const FCtx& Ctx);
	// daily jobs (QST-006)
	TOptional<FBrixJob> JobOffer(int32 I, int32 Day) const;
	bool TakeJob(const FBrixJob& O);
	bool HandOver();
	int32 ResidentIndex(const FString& Rid) const { return RidIndex.FindRef(Rid, -1); }

private:
	bool Goto(const FString& Qid, const FString& Target);
	bool End(const FString& Qid, const FString& Eid);
	bool GoalMet(const FString& Qid, const FCtx& Ctx) const;
	void Note(const FString& Text);
	void FinishJob(int64 Extra = 0);
	void FailJob();
	void CheckJob(const FCtx& Ctx);
	bool No(const FString& Msg) { Why = Msg; return false; }
	TArray<int32> Shops, Homes;
	TMap<FString, int32> RidIndex;
};
