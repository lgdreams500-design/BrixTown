#include "BrixBoards.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	constexpr double KmhToMs = 1 / 3.6;
	TOptional<FVector3f> BoardColour(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O->TryGetArrayField(Key, A) || A->Num() < 3) return {};
		return FVector3f((float)(*A)[0]->AsNumber(), (float)(*A)[1]->AsNumber(), (float)(*A)[2]->AsNumber());
	}
	TArray<FString> Strings(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O->TryGetArrayField(Key, A)) for (const TSharedPtr<FJsonValue>& V : *A) Out.Add(V->AsString());
		return Out;
	}
}

bool LoadBrixBoards(const FString& Path, FBrixBoardRules& Out, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("Cannot read ") + Path; return false; }
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()) { OutError = Path + TEXT(" is not valid JSON"); return false; }
	Out = FBrixBoardRules();
	Out.Order = Strings(J, TEXT("order"));
	Out.AutoOwned = Strings(J, TEXT("auto_owned"));
	J->TryGetStringField(TEXT("auto_equipped"), Out.AutoEquipped);
	J->TryGetNumberField(TEXT("indoor_top_kmh"), Out.IndoorTopKmh);
	J->TryGetNumberField(TEXT("mall_top_kmh"), Out.MallTopKmh);
	J->TryGetNumberField(TEXT("push_m"), Out.PushM);
	J->TryGetNumberField(TEXT("push_per_s"), Out.PushPerS);
	J->TryGetNumberField(TEXT("turn_rate"), Out.TurnRate);
	J->TryGetNumberField(TEXT("cam_follow"), Out.CamFollow);
	const TSharedPtr<FJsonObject>* Kinds = nullptr;
	if (!J->TryGetObjectField(TEXT("kinds"), Kinds)) { OutError = Path + TEXT(" has no kinds"); return false; }
	for (const auto& It : (*Kinds)->Values)
	{
		const TSharedPtr<FJsonObject> O = It.Value->AsObject();
		FBrixBoardKind K;
		K.Id = It.Key;
		O->TryGetStringField(TEXT("name"), K.Name);
		O->TryGetNumberField(TEXT("top_kmh"), K.TopKmh);
		O->TryGetNumberField(TEXT("accel_kmh_s"), K.AccelKmhS);
		O->TryGetNumberField(TEXT("glide_kmh_s"), K.GlideKmhS);
		O->TryGetNumberField(TEXT("brake_kmh_s"), K.BrakeKmhS);
		O->TryGetBoolField(TEXT("powered"), K.bPowered);
		O->TryGetBoolField(TEXT("push"), K.bPush);
		O->TryGetBoolField(TEXT("boost"), K.bBoost);
		O->TryGetNumberField(TEXT("deck_m"), K.DeckM);
		O->TryGetNumberField(TEXT("width_m"), K.WidthM);
		O->TryGetNumberField(TEXT("top_m"), K.TopM);
		O->TryGetNumberField(TEXT("wheelbase_m"), K.WheelbaseM);
		O->TryGetNumberField(TEXT("track_m"), K.TrackM);
		O->TryGetNumberField(TEXT("wheel_m"), K.WheelM);
		if (auto C = BoardColour(O, TEXT("deck"))) K.Deck = *C;
		if (auto C = BoardColour(O, TEXT("grip"))) K.Grip = *C;
		if (auto C = BoardColour(O, TEXT("truck"))) K.Truck = *C;
		if (auto C = BoardColour(O, TEXT("wheel"))) K.Wheel = *C;
		K.Glow = BoardColour(O, TEXT("glow"));
		K.Battery = BoardColour(O, TEXT("battery"));
		K.Band = BoardColour(O, TEXT("band"));
		K.Light = BoardColour(O, TEXT("light"));
		K.Core = BoardColour(O, TEXT("core"));
		Out.Kinds.Add(K.Id, K);
	}
	for (const FString& K : Out.Order) if (!Out.Kinds.Contains(K)) { OutError = Path + TEXT(": the order names an unknown kind ") + K; return false; }
	return true;
}

double FBrixBoards::Step(double Dt, bool bGo, bool bBack, double CapKmh)
{
	const FBrixBoardKind* K = Kind();
	if (!K) return V = 0;
	const double Top = (CapKmh >= 0 ? FMath::Min(CapKmh, K->TopKmh) : K->TopKmh) * KmhToMs;
	if (V > Top) V = FMath::Max(Top, V - K->BrakeKmhS * KmhToMs * Dt);
	else if (bGo && !bBack) V = FMath::Min(Top, V + K->AccelKmhS * KmhToMs * Dt);
	else V = FMath::Max(0.0, V - (bBack ? K->BrakeKmhS : K->GlideKmhS) * KmhToMs * Dt);
	return V;
}

void FBrixBoards::Give(const TArray<FString>& List, const FString& UseK)
{
	Own.Reset();
	if (R) for (const FString& K : R->Order) if (List.Contains(K)) Own.Add(K);
	InUse = Own.Contains(UseK) ? UseK : Own.Num() ? Own.Last() : FString();
	bRidingRaw = false;
	V = 0;
}

void FBrixBoards::Use(const FString& K)
{
	if (!Own.Contains(K)) return;
	InUse = K;
	V = FMath::Min(V, R->Kind(K)->TopKmh * KmhToMs);
}

TOptional<bool> FBrixBoards::Toggle()
{
	if (!Kind()) return {};
	bRidingRaw = !bRidingRaw;
	V = 0;
	return bRidingRaw;
}
