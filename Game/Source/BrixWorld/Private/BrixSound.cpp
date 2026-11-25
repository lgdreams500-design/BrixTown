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
