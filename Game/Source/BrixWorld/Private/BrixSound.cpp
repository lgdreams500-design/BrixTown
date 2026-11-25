#include "BrixSound.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TSharedPtr<FJsonObject> ReadJson(const FString& Path, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("cannot read ") + Path; return nullptr; }
		TSharedPtr<FJsonObject> O;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) || !O.IsValid()) { OutError = Path + TEXT(" is not JSON"); return nullptr; }
		return O;
	}
	const TSharedPtr<FJsonObject>* Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* P = nullptr;
		return O.IsValid() && O->TryGetObjectField(Key, P) ? P : nullptr;
	}
	void Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double& V) { if (O.IsValid()) O->TryGetNumberField(Key, V); }
	// sound.js sm(): a smooth step from a to b
	double Smooth(double A, double B, double V) { const double T = FMath::Clamp((V - A) / (B - A), 0.0, 1.0); return T * T * (3 - 2 * T); }
}

bool LoadBrixSoundRules(const FString& Path, FBrixSoundRules& Out, FString& OutError)
{
	const TSharedPtr<FJsonObject> S = ReadJson(Path, OutError);
	if (!S) return false;
	const TSharedPtr<FJsonObject>* Mix = Obj(S, TEXT("mix"));
	const TSharedPtr<FJsonObject>* H = Obj(S, TEXT("hearing"));
	const TSharedPtr<FJsonObject>* F = Obj(S, TEXT("footsteps"));
	const TSharedPtr<FJsonObject>* P = Obj(S, TEXT("people"));
	if (!Mix || !H || !F || !P) { OutError = Path + TEXT(" lacks mix, hearing, footsteps or people"); return false; }
	Num(*Mix, TEXT("master"), Out.Master); Num(*Mix, TEXT("world"), Out.World); Num(*Mix, TEXT("voice"), Out.Voice);
	Num(*Mix, TEXT("action"), Out.Action); Num(*Mix, TEXT("music"), Out.Music);
	Num(*H, TEXT("near"), Out.Near); Num(*H, TEXT("rolloff"), Out.Rolloff); Num(*H, TEXT("far"), Out.Far); Num(*H, TEXT("pan"), Out.Pan);
	Num(*F, TEXT("player_gain"), Out.PlayerStepGain); Num(*F, TEXT("resident_gain"), Out.ResidentStepGain);
	Num(*F, TEXT("far"), Out.StepFar); Num(*F, TEXT("land_gain"), Out.LandGain);
	double Max = Out.StepMax; Num(*F, TEXT("max"), Max); Out.StepMax = (int32)Max;
	Out.Surfaces.Reset();
	if (const TSharedPtr<FJsonObject>* Su = Obj(*F, TEXT("surfaces"))) for (const auto& KV : (*Su)->Values) Out.Surfaces.Add(FString(KV.Key));
	if (!Out.Surfaces.Contains(TEXT("stone"))) { OutError = Path + TEXT(": footsteps.surfaces has no stone"); return false; }
	Num(*P, TEXT("far"), Out.PeopleFar);
	if (const TSharedPtr<FJsonObject>* St = Obj(*P, TEXT("steps")))
	{
		Num(*St, TEXT("run_louder"), Out.RunLouder); Num(*St, TEXT("child_softer"), Out.ChildSofter); Num(*St, TEXT("child_pitch"), Out.ChildPitch);
	}
	if (const TSharedPtr<FJsonObject>* Po = Obj(*P, TEXT("poses")))
	{
		(*Po)->TryGetStringField(TEXT("sit"), Out.PoseSit); (*Po)->TryGetStringField(TEXT("stand"), Out.PoseStand); (*Po)->TryGetStringField(TEXT("lie"), Out.PoseLie);
	}
	const TSharedPtr<FJsonObject>* Ve = Obj(S, TEXT("vehicles"));
	const TSharedPtr<FJsonObject>* E = Ve ? Obj(*Ve, TEXT("engine")) : nullptr;
	const TSharedPtr<FJsonObject>* Pl = Ve ? Obj(*Ve, TEXT("player")) : nullptr;
	const TSharedPtr<FJsonObject>* Tr = Ve ? Obj(*Ve, TEXT("tram")) : nullptr;
	if (!E || !Pl || !Tr) { OutError = Path + TEXT(" lacks vehicles.engine, .player or .tram"); return false; }
	Num(*E, TEXT("idle_hz"), Out.IdleHz); Num(*E, TEXT("hz_per_mps"), Out.HzPerMps); Num(*E, TEXT("max_hz"), Out.MaxHz); Num(*E, TEXT("gain"), Out.EngineGain);
	Num(*E, TEXT("far"), Out.EngineFar); Num(*E, TEXT("van_pitch"), Out.VanPitch); Num(*E, TEXT("tyres_gain"), Out.TyresGain);
	double Voices = Out.EngineVoices; Num(*E, TEXT("voices"), Voices); Out.EngineVoices = (int32)Voices;
	Num(*Pl, TEXT("gain"), Out.CarGain); Num(*Pl, TEXT("start_len"), Out.StartLen); Num(*Pl, TEXT("skid_mps"), Out.SkidMps);
	Num(*Pl, TEXT("skid_gain"), Out.SkidGain); Num(*Pl, TEXT("bump_gain"), Out.BumpGain);
	Num(*Tr, TEXT("hum_hz"), Out.HumHz); Num(*Tr, TEXT("whine_hz"), Out.WhineHz); Num(*Tr, TEXT("gain"), Out.TramGain);
	Num(*Tr, TEXT("far"), Out.TramFar); Num(*Tr, TEXT("bell_far"), Out.BellFar);
	const TSharedPtr<FJsonObject>* A = Obj(S, TEXT("ambience"));
	const TSharedPtr<FJsonObject>* St = A ? Obj(*A, TEXT("street")) : nullptr;
	const TSharedPtr<FJsonObject>* Rm = A ? Obj(*A, TEXT("room")) : nullptr;
	const TSharedPtr<FJsonObject>* Bi = A ? Obj(*A, TEXT("birds")) : nullptr;
	const TSharedPtr<FJsonObject>* Cr = A ? Obj(*A, TEXT("crickets")) : nullptr;
	const TSharedPtr<FJsonObject>* Fo = A ? Obj(*A, TEXT("fountain")) : nullptr;
	if (!St || !Rm || !Bi || !Cr || !Fo) { OutError = Path + TEXT(" lacks ambience.street, room, birds, crickets or fountain"); return false; }
	auto Hm = [&](const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double& V)   // "HH:MM" -> minutes of the day
	{
		FString T;
		if (O->TryGetStringField(Key, T) && T.Len() == 5 && T[2] == TEXT(':')) V = FCString::Atoi(*T.Left(2)) * 60 + FCString::Atoi(*T.Mid(3));
	};
	Num(*St, TEXT("day_gain"), Out.StreetDay); Num(*St, TEXT("night_gain"), Out.StreetNight); Num(*Rm, TEXT("gain"), Out.RoomGain);
	Hm(*Bi, TEXT("from"), Out.BirdsFrom); Hm(*Bi, TEXT("to"), Out.BirdsTo); Num(*Bi, TEXT("per_minute"), Out.BirdsPerMinute);
	Num(*Bi, TEXT("garden_per_minute"), Out.GardenBirdsPerMinute); Num(*Bi, TEXT("garden_m"), Out.GardenM);
	Hm(*Cr, TEXT("from"), Out.CricketsFrom); Hm(*Cr, TEXT("to"), Out.CricketsTo); Num(*Cr, TEXT("gain"), Out.CricketsGain);
	Num(*Fo, TEXT("gain"), Out.FountainGain); Num(*Fo, TEXT("far"), Out.FountainFar);
	if (const TSharedPtr<FJsonObject>* In = Obj(S, TEXT("indoors"))) { Num(*In, TEXT("muffle_hz"), Out.MuffleHz); Num(*In, TEXT("muffle_gain"), Out.MuffleGain); }
	const TSharedPtr<FJsonObject>* Mu = Obj(S, TEXT("music"));
	const TSharedPtr<FJsonObject>* Sc = Mu ? Obj(*Mu, TEXT("score")) : nullptr;
	const TSharedPtr<FJsonObject>* Pls = Mu ? Obj(*Mu, TEXT("places")) : nullptr;
	if (!Sc || !Pls) { OutError = Path + TEXT(" lacks music.score or music.places"); return false; }
	Num(*Sc, TEXT("gain"), Out.ScoreGain); Num(*Sc, TEXT("talk_duck"), Out.TalkDuck); Num(*Sc, TEXT("source_duck"), Out.SourceDuck);
	Out.Moods.Reset();
	for (const TSharedPtr<FJsonValue>& V : (*Sc)->GetArrayField(TEXT("moods")))
	{
		FBrixSoundRules::FMood M;
		V->AsObject()->TryGetStringField(TEXT("name"), M.Name);
		Hm(V->AsObject(), TEXT("from"), M.From);
		Out.Moods.Add(M);
	}
	if (!Out.Moods.Num()) { OutError = Path + TEXT(": the score has no moods"); return false; }
	Out.Places.Reset();
	for (const auto& KV : (*Pls)->Values)
	{
		const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
		FBrixSoundRules::FPlace Pl2;
		O->TryGetStringField(TEXT("style"), Pl2.Style);
		Hm(O, TEXT("from"), Pl2.From); Hm(O, TEXT("to"), Pl2.To); Num(O, TEXT("far"), Pl2.Far);
		O->TryGetBoolField(TEXT("market_days_only"), Pl2.bMarketDaysOnly);
		Out.Places.Add(FString(KV.Key), Pl2);
	}
	Out.Radio.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Ra = nullptr;
	if ((*Mu)->TryGetArrayField(TEXT("radio"), Ra))
		for (const TSharedPtr<FJsonValue>& V : *Ra)
		{
			FBrixSoundRules::FStation St2;
			V->AsObject()->TryGetStringField(TEXT("name"), St2.Name);
			V->AsObject()->TryGetStringField(TEXT("style"), St2.Style);
			V->AsObject()->TryGetStringField(TEXT("podcast"), St2.Podcast);
			Out.Radio.Add(St2);
		}
	const TSharedPtr<FJsonObject>* Rw = Obj(S, TEXT("rewards"));
	const TSharedPtr<FJsonObject>* Q = Rw ? Obj(*Rw, TEXT("quest")) : nullptr;
	const TSharedPtr<FJsonObject>* Bk = Rw ? Obj(*Rw, TEXT("interest")) : nullptr;
	const TSharedPtr<FJsonObject>* Co = Rw ? Obj(*Rw, TEXT("coins")) : nullptr;
	if (!Q || !Bk || !Co) { OutError = Path + TEXT(" lacks rewards.quest, .interest or .coins"); return false; }
	auto Ints = [](const TSharedPtr<FJsonObject>& O, const TCHAR* Key, TArray<int32>& V) { V.Reset(); const TArray<TSharedPtr<FJsonValue>>* A = nullptr; if (O->TryGetArrayField(Key, A)) for (const TSharedPtr<FJsonValue>& X : *A) V.Add((int32)X->AsNumber()); };
	double D = Out.RewardRoot; Num(*Q, TEXT("root"), D); Out.RewardRoot = (int32)D;
	D = Out.TopStep; Num(*Q, TEXT("top_step"), D); Out.TopStep = (int32)D;
	D = Out.InterestRoot; Num(*Bk, TEXT("root"), D); Out.InterestRoot = (int32)D;
	D = Out.InterestCoins; Num(*Co, TEXT("interest_coins"), D); Out.InterestCoins = (int32)D;
	Ints(*Q, TEXT("scale"), Out.RewardScale);
	Ints(*Bk, TEXT("scale"), Out.InterestScale);
	if (!Out.RewardScale.Num() || !Out.InterestScale.Num()) { OutError = Path + TEXT(": a reward scale is empty"); return false; }
	return true;
}

FVector2D BrixHear(const FBrixSoundRules& R, const FVector2D& Here, double Yaw, const FVector2D& At, double Far)
{
	const FVector2D D = At - Here;
	const double d = D.Size(), f = Far > 0 ? Far : R.Far;
	if (d > f) return FVector2D::ZeroVector;
	const double G = R.Near / (R.Near + R.Rolloff * FMath::Max(0.0, d - R.Near)) * (1 - Smooth(f * .7, f, d));
	const double Pan = d < .5 ? 0.0 : (D.X * FMath::Cos(Yaw) + D.Y * FMath::Sin(Yaw)) / d * R.Pan;
	return FVector2D(G, FMath::Clamp(Pan, -1.0, 1.0));
}

bool BrixWithin(double M, double From, double To)
{
	return From <= To ? M >= From && M < To : M >= From || M < To;
}

double BrixStreetDay(double Minute, double SunriseH, double SunsetH)
{
	return Smooth(0, 1, FMath::Min((Minute - (SunriseH * 60 - 40)) / 80, ((SunsetH * 60 + 40) - Minute) / 80));
}

void FBrixAmbience::Frame(const FBrixSoundRules& R, double Minute, double SunriseH, double SunsetH, bool bIndoors, bool bDriving, const FVector2D& Here,
	const TArray<FVector2D>& Fountains, const TArray<FVector2D>& Gardens, double Dt, FRandomStream& Rng, TArray<FBrixSoundEvent>& Out)
{
	StreetDay = BrixStreetDay(Minute, SunriseH, SunsetH);
	Street = R.StreetNight + (R.StreetDay - R.StreetNight) * StreetDay;
	Room = bIndoors ? R.RoomGain : 0;
	Crickets = BrixWithin(Minute, R.CricketsFrom, R.CricketsTo) && !bIndoors ? R.CricketsGain : 0;
	Fountain.bOn = false;
	double Best = 1e18;
	for (const FVector2D& F : Fountains)
	{
		const double d = FVector2D::DistSquared(F, Here);
		if (d < Best) { Best = d; Fountain.bOn = true; Fountain.Pos = F; }
	}
	Fountain.Gain = R.FountainGain;
	Fountain.Far = R.FountainFar;
	if (BrixWithin(Minute, R.BirdsFrom, R.BirdsTo) && !bDriving)   // now and then, somewhere round the listener; more by the gardens
	{
		const bool bGarden = Gardens.ContainsByPredicate([&](const FVector2D& G) { return FVector2D::Distance(G, Here) < R.GardenM; });
		const double Rate = (bGarden ? R.GardenBirdsPerMinute : R.BirdsPerMinute) / 60;
		BirdT -= Dt;
		if (BirdT <= 0)
		{
			const double A = Rng.FRand() * 6.283, D = Rng.FRandRange(6, 28);
			FBrixSoundEvent E;
			E.Sound = TEXT("bird");
			E.Pos = Here + FVector2D(FMath::Cos(A), FMath::Sin(A)) * D;
			E.Far = 45;
			Out.Add(E);
			BirdT = Rng.FRandRange(.3, 1.7) / Rate;
		}
	}
}

FString BrixMoodAt(const FBrixSoundRules& R, double Minute)
{
	FString Best = R.Moods.Last().Name;   // before the first mood's time: the last one (the night)
	for (const FBrixSoundRules::FMood& M : R.Moods) if (M.From <= Minute) Best = M.Name;
	return Best;
}

FBrixMusicPlace BrixMusicPlace(const FBrixSoundRules& R, const TArray<FBrixMusicLot>& Lots, const FVector2D& Here, int32 InLot, double Minute, bool bMarketDay)
{
	FBrixMusicPlace Best;
	double Bd = 1e18;
	for (const FBrixMusicLot& L : Lots)
	{
		const FBrixSoundRules::FPlace* P = R.Places.Find(L.Kind);
		if (!P || !BrixWithin(Minute, P->From, P->To) || (P->bMarketDaysOnly && !bMarketDay)) continue;
		const double d = L.Lot == InLot ? 0.0 : FVector2D(FMath::Max3(L.Min.X - Here.X, 0.0, Here.X - L.Max.X), FMath::Max3(L.Min.Y - Here.Y, 0.0, Here.Y - L.Max.Y)).Size();
		if (d < P->Far && d < Bd)
		{
			Bd = d;
			Best.Lot = L.Lot;
			Best.Style = P->Style;
			Best.Far = P->Far;
			Best.bInside = L.Lot == InLot;
			Best.bOpenAir = L.Kind == TEXT("market");   // the busker plays in the open: not muffled from the street
			Best.Pos = Best.bInside ? Here : L.Door;
		}
	}
	return Best;
}

double BrixScoreLevel(const FBrixSoundRules& R, bool bTalking, double Near)
{
	return R.ScoreGain * (bTalking ? R.TalkDuck : 1.0) * (1 - (1 - R.SourceDuck) * FMath::Min(1.0, Near * 2.5));
}

int32 BrixNextStation(const FBrixSoundRules& R, int32 Station)
{
	for (int32 k = Station + 1; k <= R.Radio.Num(); ++k) if (!R.Radio[k - 1].Style.IsEmpty()) return k;
	return 0;
}

bool BrixIsMarketDay(const FString& CalendarPath, const FString& Weekday)
{
	FString Error;
	const TSharedPtr<FJsonObject> C = ReadJson(CalendarPath, Error);
	const TSharedPtr<FJsonObject>* Cal = C ? Obj(C, TEXT("calendar")) : nullptr;
	const TSharedPtr<FJsonObject> O = Cal ? *Cal : C;
	if (!O) return false;
	const TArray<TSharedPtr<FJsonValue>>* Days = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Market = nullptr;
	if (!O->TryGetArrayField(TEXT("week"), Days) || !O->TryGetArrayField(TEXT("market_days"), Market)) return false;
	const int32 i = Days->IndexOfByPredicate([&](const TSharedPtr<FJsonValue>& V) { return V->AsString() == Weekday; });
	return i >= 0 && Market->ContainsByPredicate([&](const TSharedPtr<FJsonValue>& V) { return (int32)V->AsNumber() == i; });
}

int32 BrixRewardMidi(int32 Root, const TArray<int32>& Scale, int32 K)
{
	return Root + 12 * (K / Scale.Num()) + Scale[K % Scale.Num()];
}

FString BrixRewardSound(const FBrixSoundRules& R, const FString& What, int32 I, int32 N)
{
	if (What == TEXT("coin_take"))
		return FString::Printf(TEXT("reward_coin_take_%d"), FMath::Clamp((int32)FMath::RoundHalfFromZero(I * FMath::Min(1.0, (double)R.TopStep / FMath::Max(1, N - 1))), 0, R.TopStep));
	if (What == TEXT("interest_coin")) return FString::Printf(TEXT("reward_interest_coin_%d"), FMath::Clamp(I, 0, R.InterestCoins - 1));
	return TEXT("reward_") + What;
}

FString BrixStepSurface(bool bInside, const FString& LotKind)
{
	return bInside ? TEXT("wood") : LotKind == TEXT("garden") ? TEXT("grass") : TEXT("stone");
}

void FBrixPeopleSounds::Frame(const FBrixSoundRules& R, const TArray<FBrixHeardPerson>& People, const FVector2D& Here, double Now, TArray<FBrixSoundEvent>& Out)
{
	TArray<const FBrixHeardPerson*> Order;
	for (const FBrixHeardPerson& P : People) Order.Add(&P);
	Order.StableSort([&](const FBrixHeardPerson& A, const FBrixHeardPerson& B) { return FVector2D::Distance(A.Pos, Here) < FVector2D::Distance(B.Pos, Here); });   // nearest first
	int32 Steps = 0;
	for (const FBrixHeardPerson* r : Order)
	{
		const double d = FVector2D::Distance(r->Pos, Here);
		FState* p = St.Find(r->Id);
		if (!p) { p = &St.Add(r->Id); p->Pose = r->Pose; }
		p->Seen = Now;
		if (r->bWalking && d < R.StepFar)   // steps: louder running, lighter and higher for children (SND-017)
		{
			const int32 Half = FMath::FloorToInt(r->Phase * 2);
			if (p->bHalf && Half != p->Half && Steps++ < R.StepMax)
			{
				FBrixSoundEvent E;
				E.Sound = TEXT("step_") + BrixStepSurface(r->bInside, r->LotKind) + (r->bChild ? TEXT("_child") : TEXT("_adult"));
				E.Pos = r->Pos;
				E.Gain = R.ResidentStepGain * (r->bRun ? R.RunLouder : 1.0) * (r->bChild ? R.ChildSofter : 1.0);
				E.Far = R.StepFar;
				E.Pitch = r->bChild ? R.ChildPitch : 1.0;
				Out.Add(E);
			}
			p->Half = Half;
			p->bHalf = true;
		}
		else p->bHalf = false;
		if (p->Pose != r->Pose && d < 10)   // sitting down, lying down, getting up
		{
			const FString* Name = r->Pose == 2 ? &R.PoseSit : r->Pose == 3 ? &R.PoseLie : p->Pose >= 2 ? &R.PoseStand : nullptr;
			if (Name)
			{
				FBrixSoundEvent E;
				E.Sound = TEXT("person_") + *Name;
				E.Pos = r->Pos;
				E.Far = 10;
				Out.Add(E);
			}
		}
		p->Pose = r->Pose;
	}
	if (Now - PruneT > 10)
	{
		PruneT = Now;
		for (auto It = St.CreateIterator(); It; ++It) if (Now - It->Value.Seen > 30) It.RemoveCurrent();
	}
}

void FBrixPlayerSounds::Frame(const FBrixSoundRules& R, bool bMoving, bool bOnGround, double Phase, bool bRun, double Vz, const FString& Surface, TArray<FBrixSoundEvent>& Out)
{
	const int32 Half = FMath::FloorToInt(Phase * 2);
	if (bMoving && bOnGround && bLast && Half != LastHalf)
	{
		FBrixSoundEvent E;
		E.Sound = TEXT("step_") + Surface + TEXT("_adult");
		E.Gain = R.PlayerStepGain * (bRun ? 1.4 : 1.0);
		E.Far = 5;
		E.bHere = true;
		Out.Add(E);
	}
	LastHalf = Half;
	bLast = true;
	if (!bOnGround && bPrevGround && Vz > 1)   // a jump
	{
		FBrixSoundEvent E;
		E.Sound = TEXT("act_jump");
		E.bHere = true;
		Out.Add(E);
	}
	if (bOnGround && !bPrevGround && PrevVz < -3)   // a hard landing thuds
	{
		FBrixSoundEvent E;
		E.Sound = TEXT("act_land");
		E.Gain = R.LandGain * FMath::Min(1.5, -PrevVz / 6);
		E.bHere = true;
		Out.Add(E);
	}
	bPrevGround = bOnGround;
	PrevVz = Vz;
}

void FBrixEngineSounds::Frame(const FBrixSoundRules& R, const TArray<FBrixCarHeard>& Cars, const FVector2D& Here, const FBrixDriveHeard* Drive, double Now, TArray<FBrixSoundEvent>& Out)
{
	Pool.SetNum(R.EngineVoices);
	TArray<TPair<double, const FBrixCarHeard*>> List;
	for (const FBrixCarHeard& C : Cars) { const double d = FVector2D::Distance(C.Pos, Here); if (d < R.EngineFar) List.Add({ d, &C }); }
	List.StableSort([](const TPair<double, const FBrixCarHeard*>& A, const TPair<double, const FBrixCarHeard*>& B) { return A.Key < B.Key; });
	for (int32 k = 0; k < Pool.Num(); ++k)
	{
		FBrixEngineVoice& V = Pool[k];
		V.bOn = k < List.Num();
		if (!V.bOn) continue;
		const FBrixCarHeard& C = *List[k].Value;
		const double v = FMath::Abs(C.V);
		V.Pos = C.Pos;
		V.Hz = FMath::Min(R.MaxHz, R.IdleHz + v * R.HzPerMps) * (C.bVan ? R.VanPitch : 1.0);
		V.Tyres = FMath::Min(1.0, v / 10) * R.TyresGain * 8;
		V.Gain = R.EngineGain * (.55 + .45 * FMath::Min(1.0, v / 8));
		V.Far = R.EngineFar;
	}
	// the player's own car: louder, not muffled, with skids and bumps (frame(): drive = { since } when it begins)
	if (Drive && !Since) Since = Now;
	else if (!Drive) Since.Reset();
	if (Drive)
	{
		const double v = FMath::Abs(Drive->V), Running = Now - *Since;
		const bool bStarted = Running > R.StartLen;
		Mine.bOn = true;
		Mine.Hz = bStarted ? FMath::Min(R.MaxHz * 1.2, R.IdleHz * 1.1 + v * R.HzPerMps * 1.1 + (Drive->bGas ? 8 : 0)) : R.IdleHz * (.4 + .6 * Running / R.StartLen);
		Mine.Tyres = v / 14 * R.TyresGain * 6;
		Mine.Gain = bStarted ? R.CarGain * (.7 + .3 * FMath::Min(1.0, v / 8)) : R.CarGain * .6;
		Skid = Drive->bBrake && v > R.SkidMps ? R.SkidGain : 0;
		if (PrevV && FMath::Abs(*PrevV) > 2 && FMath::Sign(*PrevV) != FMath::Sign(Drive->V) && FMath::Abs(Drive->V) < FMath::Abs(*PrevV) * .5)
		{
			FBrixSoundEvent Ev;   // stopped short: a bump (the recipe of sound.js engines(), recorded as car_bump)
			Ev.Sound = TEXT("car_bump");
			Ev.bHere = true;
			Out.Add(Ev);
		}
	}
	else { Mine.bOn = false; Mine.Gain = 0; Skid = 0; }
	if (Drive) PrevV = Drive->V; else PrevV.Reset();
}

void FBrixTramSounds::Frame(const FBrixSoundRules& R, const TArray<FBrixTramHeard>& Trams, double Z, const FVector2D& Here, double Now, TArray<FBrixSoundEvent>& Out)
{
	TArray<TPair<double, const FBrixTramHeard*>> Near;   // nearest first, by the distance along and across
	for (const FBrixTramHeard& T : Trams) Near.Add({ FMath::Abs(T.X - Here.X) + FMath::Abs(Z - Here.Y), &T });
	Near.StableSort([](const TPair<double, const FBrixTramHeard*>& A, const TPair<double, const FBrixTramHeard*>& B) { return A.Key < B.Key; });
	auto Find = [&](double Run) { return Seen.IndexOfByPredicate([&](const FSeen& S) { return S.Run == Run; }); };
	for (int32 k = 0; k < 2; ++k)
	{
		FBrixTramVoice& V = Voices[k];
		V.bOn = k < Near.Num();
		if (!V.bOn) continue;
		const FBrixTramHeard& T = *Near[k].Value;
		const int32 Prev = Find(T.Run);
		const bool bMoving = !T.bStopped;
		const double Sp = Prev >= 0 ? FMath::Min(1.0, FMath::Abs(T.X - Seen[Prev].X) / FMath::Max(.001, Now - Seen[Prev].T) / 8) : 0;
		V.Pos = FVector2D(T.X, Z);
		V.Gain = R.TramGain * (bMoving ? .6 + .4 * Sp : .35);
		V.Far = R.TramFar;
		V.WhineHz = R.WhineHz * (.5 + Sp);
		V.Whine = bMoving ? .12 * Sp : 0;
		V.Rumble = bMoving ? .9 * Sp + .1 : 0;
	}
	for (const FBrixTramHeard& T : Trams)
	{
		const int32 Prev = Find(T.Run);
		if (Prev >= 0 && Seen[Prev].bStopped != T.bStopped)   // the bell as it stops and as it leaves
		{
			FBrixSoundEvent E;
			E.Sound = TEXT("tram_bell");
			E.Pos = FVector2D(T.X, Z);
			E.Far = R.BellFar;
			Out.Add(E);
		}
		if (Prev < 0) Seen.Add({ T.Run, T.X, Now, T.bStopped });
		else if (Seen[Prev].bStopped != T.bStopped || Now - Seen[Prev].T > .25) Seen[Prev] = { T.Run, T.X, Now, T.bStopped };
	}
	if (Seen.Num() > 30) Seen.RemoveAt(0, 10);
}

bool LoadBrixSoundIndex(const FString& Path, TMap<FString, FBrixClipInfo>& Out, TArray<double>& OutEngineHz, FString& OutError)
{
	const TSharedPtr<FJsonObject> S = ReadJson(Path, OutError);
	if (!S) return false;
	const TSharedPtr<FJsonObject>* Sounds = Obj(S, TEXT("sounds"));
	if (!Sounds) { OutError = Path + TEXT(" has no sounds"); return false; }
	for (const auto& KV : (*Sounds)->Values)
	{
		const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
		if (!O.IsValid()) continue;
		FBrixClipInfo C;
		O->TryGetStringField(TEXT("file"), C.File);
		O->TryGetBoolField(TEXT("loop"), C.bLoop);
		O->TryGetNumberField(TEXT("seconds"), C.Seconds);
		O->TryGetNumberField(TEXT("scale"), C.Scale);
		if (C.File.IsEmpty() || C.Scale <= 0) { OutError = Path + TEXT(": ") + FString(KV.Key) + TEXT(" has no file or scale"); return false; }
		Out.Add(FString(KV.Key), C);
	}
	OutEngineHz.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Hz = nullptr;
	if (S->TryGetArrayField(TEXT("engine_hz"), Hz)) for (const TSharedPtr<FJsonValue>& V : *Hz) OutEngineHz.Add(V->AsNumber());
	return true;
}

bool ReadBrixWav(const TArray<uint8>& File, TArray<uint8>& OutPcm, int32& OutRate, int32& OutChannels)
{
	auto U32 = [&](int32 At) { return (uint32)File[At] | (uint32)File[At + 1] << 8 | (uint32)File[At + 2] << 16 | (uint32)File[At + 3] << 24; };
	auto U16 = [&](int32 At) { return (uint16)(File[At] | File[At + 1] << 8); };
	if (File.Num() < 12 || FMemory::Memcmp(File.GetData(), "RIFF", 4) || FMemory::Memcmp(File.GetData() + 8, "WAVE", 4)) return false;
	bool bFormat = false;
	for (int32 At = 12; At + 8 <= File.Num();)
	{
		const uint32 Size = U32(At + 4);
		const int32 Body = At + 8;
		if ((int64)Body + Size > File.Num()) return false;
		if (!FMemory::Memcmp(File.GetData() + At, "fmt ", 4))
		{
			if (Size < 16 || U16(Body) != 1 || U16(Body + 14) != 16) return false;   // 16-bit PCM only
			OutChannels = U16(Body + 2);
			OutRate = (int32)U32(Body + 4);
			bFormat = true;
		}
		else if (!FMemory::Memcmp(File.GetData() + At, "data", 4))
		{
			if (!bFormat) return false;
			OutPcm = TArray<uint8>(File.GetData() + Body, (int32)Size);
			return OutChannels > 0 && OutRate > 0;
		}
		At = Body + (int32)Size + (Size & 1);
	}
	return false;
}
