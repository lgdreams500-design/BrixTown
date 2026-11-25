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
	Num(*Pl, TEXT("skid_gain"), Out.SkidGain); Num(*Pl, TEXT("bump_gain"), Out.BumpGain); Num(*Pl, TEXT("horn_gain"), Out.HornGain);
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
	auto Pair = [](const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double V[2]) { const TArray<TSharedPtr<FJsonValue>>* A = nullptr; if (O->TryGetArrayField(Key, A) && A->Num() == 2) { V[0] = (*A)[0]->AsNumber(); V[1] = (*A)[1]->AsNumber(); } };
	auto Strs = [](const TSharedPtr<FJsonObject>& O, const TCHAR* Key, TArray<FString>& V) { V.Reset(); const TArray<TSharedPtr<FJsonValue>>* A = nullptr; if (O->TryGetArrayField(Key, A)) for (const TSharedPtr<FJsonValue>& X : *A) V.Add(X->AsString()); };
	const TSharedPtr<FJsonObject>* Vo = Obj(S, TEXT("voices"));
	if (!Vo) { OutError = Path + TEXT(" lacks voices"); return false; }
	if (const TSharedPtr<FJsonObject>* Pi = Obj(*Vo, TEXT("pitch"))) { Num(*Pi, TEXT("min"), Out.VoiceMin); Num(*Pi, TEXT("max"), Out.VoiceMax); }
	Num(*Vo, TEXT("line_gain"), Out.LineGain);
	Out.VoiceShapes.Reset(); Out.TalkShapes.Reset();
	if (const TSharedPtr<FJsonObject>* Sh = Obj(*Vo, TEXT("shapes"))) for (const auto& KV : (*Sh)->Values) { Out.VoiceShapes.Add(FString(KV.Key)); if (FString(KV.Key) != TEXT("MBP")) Out.TalkShapes.Add(FString(KV.Key)); }
	if (!Out.TalkShapes.Num()) { OutError = Path + TEXT(": voices.shapes is empty"); return false; }
	Out.RolePitch.Reset();
	if (const TSharedPtr<FJsonObject>* Rp = Obj(*P, TEXT("pitch"))) for (const auto& KV : (*Rp)->Values) { const TArray<TSharedPtr<FJsonValue>>& Pr = KV.Value->AsArray(); if (Pr.Num() == 2) Out.RolePitch.Add(FString(KV.Key), FVector2D(Pr[0]->AsNumber(), Pr[1]->AsNumber())); }
	double Busy = Out.Busy; Num(*P, TEXT("busy"), Busy); Out.Busy = (int32)Busy;
	Num(*P, TEXT("call_gain"), Out.CallGain); Num(*P, TEXT("shout_gain"), Out.ShoutGain);
	if (const TSharedPtr<FJsonObject>* Ch = Obj(*P, TEXT("chat")))
	{
		TArray<FString> Ca; Strs(*Ch, TEXT("acts"), Ca); Out.ChatActs = TSet<FString>(Ca);
		Num(*Ch, TEXT("pair_m"), Out.PairM); Num(*Ch, TEXT("far"), Out.ChatFar); Num(*Ch, TEXT("gain"), Out.ChatGain); Num(*Ch, TEXT("syllables_per_s"), Out.SyllablesPerS);
		Num(*Ch, TEXT("question"), Out.Question); Num(*Ch, TEXT("laugh"), Out.Laugh); Num(*Ch, TEXT("child_laugh"), Out.ChildLaugh); Num(*Ch, TEXT("laugh_gain"), Out.LaughGain);
		double G = Out.ChatGroups; Num(*Ch, TEXT("groups"), G); Out.ChatGroups = (int32)G;
		double Sy[2] = { (double)Out.Syllables[0], (double)Out.Syllables[1] }; Pair(*Ch, TEXT("syllables"), Sy); Out.Syllables[0] = (int32)Sy[0]; Out.Syllables[1] = (int32)Sy[1];
		Pair(*Ch, TEXT("turn_gap_s"), Out.TurnGap); Pair(*Ch, TEXT("pause_s"), Out.Pause);
	}
	if (const TSharedPtr<FJsonObject>* Cw = Obj(*P, TEXT("crowd")))
	{
		Num(*Cw, TEXT("min"), Out.CrowdMin); Num(*Cw, TEXT("full"), Out.CrowdFull); Num(*Cw, TEXT("far"), Out.CrowdFar); Num(*Cw, TEXT("gain"), Out.CrowdGain); Num(*Cw, TEXT("per_s"), Out.CrowdPerS);
	}
	if (const TSharedPtr<FJsonObject>* Gr = Obj(*P, TEXT("greet")))
	{
		Num(*Gr, TEXT("m"), Out.GreetM); Num(*Gr, TEXT("chance"), Out.GreetChance); Num(*Gr, TEXT("every_s"), Out.GreetEvery); Num(*Gr, TEXT("gain"), Out.GreetGain);
	}
	Out.Activities.Reset(); Out.ByAct.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Acs = nullptr;
	if ((*P)->TryGetArrayField(TEXT("activities"), Acs))
		for (const TSharedPtr<FJsonValue>& V : *Acs)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FBrixSoundRules::FActivity Ac;
			Strs(O, TEXT("acts"), Ac.Acts); Strs(O, TEXT("roles"), Ac.Roles); Pair(O, TEXT("every"), Ac.Every);
			O->TryGetStringField(TEXT("sound"), Ac.Sound); O->TryGetBoolField(TEXT("call"), Ac.bCall); O->TryGetNumberField(TEXT("chance"), Ac.Chance);
			const int32 k = Out.Activities.Add(Ac);
			for (const FString& Name : Ac.Acts) Out.ByAct.FindOrAdd(Name).Add(k);
		}
	if (const TSharedPtr<FJsonObject>* Pc = Obj(S, TEXT("podcasts")))
	{
		Num(*Pc, TEXT("gap_s"), Out.PodGapS); Num(*Pc, TEXT("intro_s"), Out.PodIntroS); Num(*Pc, TEXT("bed_gain"), Out.PodBedGain); Num(*Pc, TEXT("voice_gain"), Out.PodVoiceGain);
		Out.Shows.Reset();
		if (const TSharedPtr<FJsonObject>* Sw = Obj(*Pc, TEXT("shows")))
			for (const auto& KV : (*Sw)->Values)
			{
				const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
				FBrixSoundRules::FShow Show;
				O->TryGetStringField(TEXT("title"), Show.Title); O->TryGetStringField(TEXT("bed"), Show.Bed);
				if (const TSharedPtr<FJsonObject>* Ho = Obj(O, TEXT("hosts")))
					for (const auto& Hv : (*Ho)->Values) { FBrixSoundRules::FHost Host; Hv.Value->AsObject()->TryGetStringField(TEXT("name"), Host.Name); Num(Hv.Value->AsObject(), TEXT("pitch"), Host.Pitch); Show.Hosts.Add(FString(Hv.Key), Host); }
				for (const TSharedPtr<FJsonValue>& Ev : O->GetArrayField(TEXT("episodes")))
				{
					FBrixSoundRules::FEpisode Ep;
					Ev->AsObject()->TryGetStringField(TEXT("title"), Ep.Title);
					for (const TSharedPtr<FJsonValue>& Ln : Ev->AsObject()->GetArrayField(TEXT("lines"))) Ep.Lines.Add({ Ln->AsArray()[0]->AsString(), Ln->AsArray()[1]->AsString() });
					Show.Episodes.Add(Ep);
				}
				Out.Shows.Add(FString(KV.Key), Show);
			}
	}
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
	for (int32 k = Station + 1; k <= R.Radio.Num(); ++k) if (!R.Radio[k - 1].Style.IsEmpty() || R.Shows.Contains(R.Radio[k - 1].Podcast)) return k;
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

double BrixHashOf(int64 I)
{
	uint32 H = (uint32)(((uint64)(I + 1) * 2654435761ull) & 0xffffffffull);   // (i + 1) * 2654435761 >>> 0
	H ^= H >> 15;
	H = H * 2246822519u;                                                         // Math.imul(h, 2246822519) >>> 0
	H ^= H >> 13;
	return H / 4294967296.0;
}

double BrixVoiceHz(const FBrixSoundRules& R, int32 Resident, const FString& Role)
{
	if (const FVector2D* P = R.RolePitch.Find(Role)) return P->X + BrixHashOf(Resident) * (P->Y - P->X);
	return R.VoiceMin + BrixHashOf(Resident) * (R.VoiceMax - R.VoiceMin);
}

double BrixPhrase(const FBrixSoundRules& R, double T, double Base, int32 N, double Gain, bool bAsk, FRandomStream& Rng, TArray<FBrixSyllable>& Out)
{
	for (int32 k = 0; k < N; ++k)
	{
		const double Len = 1 / R.SyllablesPerS * Rng.FRandRange(.6, 1.25), F = (double)k / FMath::Max(1, N - 1), C = bAsk ? 1 + .22 * F * F : 1.06 - .12 * F;
		Out.Add({ T, R.TalkShapes[Rng.RandHelper(R.TalkShapes.Num())], Base * C * Rng.FRandRange(.96, 1.04), Len * .8, Gain });
		T += Len;
	}
	return T;
}

double BrixLaugh(const FBrixSoundRules& R, double T, double Base, double Gain, FRandomStream& Rng, TArray<FBrixSyllable>& Out)
{
	const int32 N = 3 + Rng.RandHelper(3);
	for (int32 k = 0; k < N; ++k) { Out.Add({ T, k % 2 ? TEXT("EH") : TEXT("AA"), Base * (1.3 - k * .06), .09, Gain * (1 - k * .13) }); T += Rng.FRandRange(.13, .17); }
	return T;
}

void BrixVoiceLine(const FBrixSoundRules& R, const FBrixLine& Line, double T0, double Base, double Gain, double From, TArray<FBrixSyllable>& Out)
{
	const double Start = T0 - From;
	for (const FBrixLine::FShape& S : Line.Seq)
	{
		if (S.T0 < From || !R.VoiceShapes.Contains(S.Shape) || (S.Shape == TEXT("MBP") && S.D < .05)) continue;
		const FBrixLine::FWord* W = nullptr;
		for (const FBrixLine::FWord& X : Line.Words) if (X.T0 <= S.T0) W = &X;
		bool bUp = false;
		for (double A : Line.Asks) bUp |= A > S.T0 && A - S.T0 < .35;
		Out.Add({ Start + S.T0, S.Shape, Base * (bUp ? 1.18 : 1.0) * (W && W->bLong ? 1.08 : 1.0), FMath::Min(.22, S.D), Gain });
	}
}

void FBrixPeopleTalk::Frame(const FBrixSoundRules& R, const TArray<FBrixHeardPerson>& People, const FVector2D& Here, double Now, double Dt, bool bGreet,
	const FBrixSpeech* Speech, FRandomStream& Rng, TArray<FBrixSoundEvent>& OutSounds, TArray<FBrixVoiceOut>& OutVoices)
{
	auto D = [&](const FBrixHeardPerson& P) { return FVector2D::Distance(P.Pos, Here); };
	TArray<const FBrixHeardPerson*> Order;
	for (const FBrixHeardPerson& P : People) Order.Add(&P);
	Order.StableSort([&](const FBrixHeardPerson& A, const FBrixHeardPerson& B) { return D(A) < D(B); });   // nearest first
	int32 Busy = 0;
	for (const FBrixHeardPerson* r : Order)
	{
		const double d = D(*r);
		FState* p = St.Find(r->Id);
		if (!p) { p = &St.Add(r->Id); p->Act = r->Act; }
		p->Seen = Now;
		if (p->Act != r->Act) { p->Act = r->Act; p->Next.Reset(); }
		const TArray<int32>* Doing = !r->bWalking && d < R.PeopleFar ? R.ByAct.Find(r->Act) : nullptr;
		if (Doing) ++Busy;
		if (Doing && Busy <= R.Busy)   // what they are doing, now and then (the nearest few only)
			for (int32 k : *Doing)
			{
				const FBrixSoundRules::FActivity& A = R.Activities[k];
				if ((A.Roles.Num() && !A.Roles.Contains(r->Role)) || (A.Chance >= 0 && BrixHashOf((int64)r->Id * 7 + 3) > A.Chance)) continue;
				double* Due = p->Next.Find(k);
				if (!Due) { p->Next.Add(k, Now + Rng.FRandRange(0, A.Every[1])); continue; }
				if (Now < *Due) continue;
				*Due = Now + Rng.FRandRange(A.Every[0], A.Every[1]);
				if (!A.Sound.IsEmpty()) { FBrixSoundEvent E; E.Sound = TEXT("pact_") + A.Sound; E.Pos = r->Pos; E.Far = R.PeopleFar; OutSounds.Add(E); }
				if (A.bCall) { FBrixVoiceOut V; V.Pos = r->Pos; V.Far = 30; BrixPhrase(R, .05, BrixVoiceHz(R, r->Id, r->Role) * 1.15, 2 + Rng.RandHelper(2), R.CallGain, false, Rng, V.Syllables); OutVoices.Add(V); }
			}
		if (!r->Shout.IsEmpty() && r->Shout != p->Shout && Speech)   // told off: the shout, voiced (CITY-071)
		{
			FBrixVoiceOut V; V.Pos = r->Pos; V.Far = 40;
			BrixVoiceLine(R, Speech->Speak(r->Shout), .02, BrixVoiceHz(R, r->Id, r->Role) * 1.2, R.ShoutGain, 0, V.Syllables);
			OutVoices.Add(V);
		}
		p->Shout = r->Shout;
		if (bGreet && r->Pose < 2 && d < R.GreetM && Now - p->Greet > R.GreetEvery)   // a passer-by says hello as the player goes by
		{
			p->Greet = Now;
			if (Rng.FRand() < R.GreetChance) { FBrixVoiceOut V; V.Pos = r->Pos; V.Far = 10; BrixPhrase(R, .1, BrixVoiceHz(R, r->Id, r->Role) * 1.1, 2, R.GreetGain, false, Rng, V.Syllables); OutVoices.Add(V); }
		}
	}
	// conversations: people standing or sitting together take turns to talk, and now and then laugh (SND-006)
	TArray<const FBrixHeardPerson*> Can;
	for (const FBrixHeardPerson& r : People) if (!r.bWalking && r.Pose != 3 && R.ChatActs.Contains(r.Act) && D(r) < R.ChatFar) Can.Add(&r);
	TSet<int32> Used;
	TArray<TArray<const FBrixHeardPerson*>> Found;
	for (const FBrixHeardPerson* r : Can)
	{
		if (Used.Contains(r->Id)) continue;
		TArray<const FBrixHeardPerson*> G = { r };
		Used.Add(r->Id);
		for (int32 k = 0; k < G.Num(); ++k)
			for (const FBrixHeardPerson* o : Can) if (!Used.Contains(o->Id) && FVector2D::Distance(o->Pos, G[k]->Pos) < R.PairM) { G.Add(o); Used.Add(o->Id); }
		if (G.Num() > 1) Found.Add(G);
	}
	Found.StableSort([&](const TArray<const FBrixHeardPerson*>& A, const TArray<const FBrixHeardPerson*>& B) { return D(*A[0]) < D(*B[0]); });
	TSet<FString> Live;
	Chat = 0; Murmur = 0;
	for (int32 gi = 0; gi < Found.Num() && gi < R.ChatGroups; ++gi)
	{
		const TArray<const FBrixHeardPerson*>& G = Found[gi];
		TArray<int32> Ids;
		for (const FBrixHeardPerson* r : G) Ids.Add(r->Id);
		Ids.Sort();
		FString Key;
		for (int32 i : Ids) Key += FString::FromInt(i) + TEXT(",");
		Live.Add(Key); ++Chat; Murmur += G.Num();
		FGroup* Gs = Groups.Find(Key);
		if (!Gs) { Gs = &Groups.Add(Key); Gs->Turn = Rng.RandHelper(G.Num()); Gs->Next = Now + Rng.FRandRange(0, 1.5); }
		if (Now < Gs->Next) continue;
		const FBrixHeardPerson* Who = G[Gs->Turn % G.Num()];
		FBrixVoiceOut V; V.Pos = Who->Pos; V.Far = R.ChatFar;
		double End = BrixPhrase(R, .02, BrixVoiceHz(R, Who->Id, Who->Role), R.Syllables[0] + Rng.RandHelper(R.Syllables[1] - R.Syllables[0] + 1), R.ChatGain, Rng.FRand() < R.Question, Rng, V.Syllables);
		OutVoices.Add(V);
		const bool bKids = G.ContainsByPredicate([](const FBrixHeardPerson* r) { return r->Role == TEXT("child"); });
		if (Rng.FRand() < (bKids ? R.ChildLaugh : R.Laugh))   // someone else in the group laughs at it
		{
			const FBrixHeardPerson* O = G[(Gs->Turn + 1) % G.Num()];
			FBrixVoiceOut L; L.Pos = O->Pos; L.Far = R.ChatFar;
			End = FMath::Max(End, BrixLaugh(R, End + .05, BrixVoiceHz(R, O->Id, O->Role), R.LaughGain, Rng, L.Syllables));
			OutVoices.Add(L);
		}
		++Gs->Turn;
		Gs->Next = Now + End + (Gs->Turn % (2 + Rng.RandHelper(2)) == 0 ? Rng.FRandRange(R.Pause[0], R.Pause[1]) : Rng.FRandRange(R.TurnGap[0], R.TurnGap[1]));
	}
	for (auto It = Groups.CreateIterator(); It; ++It) if (!Live.Contains(It->Key)) It.RemoveCurrent();
	// the hum of a crowd: where many are gathered, a low bed and syllables from among them
	TArray<const FBrixHeardPerson*> Near;
	for (const FBrixHeardPerson& r : People) if (!r.bWalking && r.Pose != 3 && D(r) < R.CrowdFar) Near.Add(&r);
	const double T = FMath::Clamp((Near.Num() - R.CrowdMin) / (R.CrowdFull - R.CrowdMin), 0.0, 1.0), K = T * T * (3 - 2 * T);
	Crowd = Near.Num();
	CrowdBed = R.CrowdGain * K * .5;
	CrowdT -= Dt;
	if (K > 0 && CrowdT <= 0)
	{
		const FBrixHeardPerson* r = Near[Rng.RandHelper(Near.Num())];
		FBrixVoiceOut V; V.Pos = r->Pos; V.Far = R.CrowdFar;
		BrixPhrase(R, .02, BrixVoiceHz(R, r->Id, r->Role), 2 + Rng.RandHelper(4), R.CrowdGain * (.5 + .5 * K), Rng.FRand() < .2, Rng, V.Syllables);
		OutVoices.Add(V);
		CrowdT = Rng.FRandRange(.5, 1.5) / (R.CrowdPerS * (1 + Near.Num() / 4.0));
	}
	if (Now - PruneT > 10) { PruneT = Now; for (auto It = St.CreateIterator(); It; ++It) if (Now - It->Value.Seen > 30) It.RemoveCurrent(); }
}

FBrixOnAir BrixPodcastAt(const FBrixSoundRules& R, const FBrixSpeech& Speech, const FString& ShowId, double Minute, double Pace)
{
	FBrixOnAir A;
	const FBrixSoundRules::FShow* Show = R.Shows.Find(ShowId);
	if (!Show || !Show->Episodes.Num()) return A;
	// the plan: every part of the show in order (plan()); worked out once per show
	struct FItem { double T0, D; int32 Ep, Line; FBrixLine Ln; };
	static TMap<FString, TPair<TArray<FItem>, double>> Plans;   // by the show and its text, so changed rules make a new plan
	FString PlanKey = ShowId + Show->Title;
	for (const FBrixSoundRules::FEpisode& E : Show->Episodes) for (const auto& L : E.Lines) PlanKey += L.Value;
	TPair<TArray<FItem>, double>* Plan = Plans.Find(PlanKey);
	if (!Plan)
	{
		Plan = &Plans.Add(PlanKey);
		double t = 0;
		for (int32 e = 0; e < Show->Episodes.Num(); ++e)
		{
			Plan->Key.Add({ t, R.PodIntroS, e, -1, FBrixLine() }); t += R.PodIntroS;
			for (int32 k = 0; k < Show->Episodes[e].Lines.Num(); ++k)
			{
				const FBrixLine Ln = Speech.Speak(Show->Episodes[e].Lines[k].Value);
				Plan->Key.Add({ t, Ln.End + R.PodGapS, e, k, Ln }); t += Ln.End + R.PodGapS;
			}
		}
		Plan->Value = t;
	}
	const double Total = Plan->Value, Pos = FMath::Fmod(FMath::Fmod(Minute * Pace, Total) + Total, Total);
	const FItem* It = &Plan->Key[0];
	for (const FItem& I : Plan->Key) if (Pos >= I.T0 && Pos < I.T0 + I.D) { It = &I; break; }
	const FBrixSoundRules::FEpisode& Ep = Show->Episodes[It->Ep];
	A.Key = ShowId + TEXT("/") + FString::FromInt(It->Ep) + TEXT(":") + (It->Line < 0 ? FString(TEXT("intro")) : FString::FromInt(It->Line));
	A.Head = Show->Title + TEXT(" \u00b7 ") + Ep.Title;
	A.bIntro = It->Line < 0;
	A.From = Pos - It->T0;
	if (!A.bIntro)
	{
		const FBrixSoundRules::FHost* H = Show->Hosts.Find(Ep.Lines[It->Line].Key);
		A.Text = (H ? H->Name : Ep.Lines[It->Line].Key) + TEXT(": ") + Ep.Lines[It->Line].Value;
		A.Pitch = H ? H->Pitch : 180;
		A.Line = It->Ln;
	}
	else A.Text = TEXT("\u2026");
	return A;
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

void LoadBrixSyllableIndex(const FString& Path, TArray<double>& OutHz, TArray<double>& OutLen)
{
	FString Error;
	OutHz.Reset(); OutLen.Reset();
	const TSharedPtr<FJsonObject> S = ReadJson(Path, Error);
	if (!S) return;
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (S->TryGetArrayField(TEXT("syllable_hz"), A)) for (const TSharedPtr<FJsonValue>& V : *A) OutHz.Add(V->AsNumber());
	if (S->TryGetArrayField(TEXT("syllable_len"), A)) for (const TSharedPtr<FJsonValue>& V : *A) OutLen.Add(V->AsNumber());
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
