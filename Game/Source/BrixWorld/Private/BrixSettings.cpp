#include "BrixSettings.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	FString K(const auto& Key) { return FString(Key.ToView()); }
	bool IsNum(const TSharedPtr<FJsonValue>& V) { return V.IsValid() && V->Type == EJson::Number; }
	bool IsHex(const FString& S)
	{
		if (S.Len() != 7 || S[0] != '#') return false;
		for (int32 i = 1; i < 7; ++i) if (!FChar::IsHexDigit(S[i])) return false;
		return true;
	}
}

TArray<FString> BrixSettingsProblems(const TSharedPtr<FJsonObject>& S)
{
	TArray<FString> Out;
	if (!S) return { TEXT("no settings") };
	for (const auto& G : S->GetObjectField(TEXT("groups"))->Values)
		for (const auto& Sv : G.Value->AsObject()->GetObjectField(TEXT("settings"))->Values)
		{
			const TSharedPtr<FJsonObject> O = Sv.Value->AsObject();
			const FString Where = K(G.Key) + TEXT(".") + K(Sv.Key);
			FString T;
			O->TryGetStringField(TEXT("type"), T);
			const TSharedPtr<FJsonValue> D = O->TryGetField(TEXT("default"));
			if (T == TEXT("switch")) { if (!D.IsValid() || D->Type != EJson::Boolean) Out.Add(TEXT("setting ") + Where + TEXT(": a switch's default is yes or no")); }
			else if (T == TEXT("choice"))
			{
				const TSharedPtr<FJsonObject>* Opts = nullptr;
				FString Dv;
				if (!D.IsValid() || !D->TryGetString(Dv) || !O->TryGetObjectField(TEXT("options"), Opts) || !(*Opts)->HasField(Dv)) Out.Add(TEXT("setting ") + Where + TEXT(": default is not one of its options"));
			}
			else if (T == TEXT("range"))
			{
				const TSharedPtr<FJsonValue> Lo = O->TryGetField(TEXT("min")), Hi = O->TryGetField(TEXT("max")), St = O->TryGetField(TEXT("step"));
				if (!IsNum(Lo) || !IsNum(Hi) || !IsNum(St) || !IsNum(D) || !(Lo->AsNumber() <= D->AsNumber() && D->AsNumber() <= Hi->AsNumber()) || St->AsNumber() <= 0)
					Out.Add(TEXT("setting ") + Where + TEXT(": a range needs min <= default <= max and a step"));
				else
				{
					const double q = (D->AsNumber() - Lo->AsNumber()) / St->AsNumber();
					if (FMath::Abs(q - FMath::RoundToDouble(q)) > 1e-6) Out.Add(TEXT("setting ") + Where + TEXT(": default is not on its steps"));
				}
			}
			else Out.Add(TEXT("setting ") + Where + TEXT(": no type ") + T);
			FString Name;
			if (!O->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) Out.Add(TEXT("setting ") + Where + TEXT(" has no name"));
		}
	const TSharedPtr<FJsonObject> A = S->GetObjectField(TEXT("actions")), C = S->GetObjectField(TEXT("contexts")), M = S->GetObjectField(TEXT("menu"));
	for (const auto& X : A->Values) { FString Ctx; X.Value->AsObject()->TryGetStringField(TEXT("context"), Ctx); if (!C->HasField(Ctx)) Out.Add(TEXT("action ") + K(X.Key) + TEXT(": no context ") + Ctx); }
	TArray<FString> GroupIds;
	const TArray<TSharedPtr<FJsonValue>>* Mg = nullptr;
	if (M->TryGetArrayField(TEXT("groups"), Mg)) for (const auto& G : *Mg) GroupIds.Add(G->AsObject()->GetStringField(TEXT("id")));
	const TSharedPtr<FJsonObject>* Colours = nullptr;
	M->TryGetObjectField(TEXT("colours"), Colours);
	for (const auto& Tv : M->GetArrayField(TEXT("tabs")))
	{
		const TSharedPtr<FJsonObject> T = Tv->AsObject();
		FString Id, Act, Grp, Col, Board = TEXT("big");
		T->TryGetStringField(TEXT("id"), Id); T->TryGetStringField(TEXT("action"), Act); T->TryGetStringField(TEXT("group"), Grp); T->TryGetStringField(TEXT("colour"), Col); T->TryGetStringField(TEXT("board"), Board);
		if (!A->HasField(Act)) Out.Add(TEXT("menu tab ") + Id + TEXT(": no action ") + Act);
		if (!GroupIds.Contains(Grp)) Out.Add(TEXT("menu tab ") + Id + TEXT(": no group ") + Grp);
		if (!Colours || !(*Colours)->HasField(Col)) Out.Add(TEXT("menu tab ") + Id + TEXT(": no colour ") + Col);
		if (Board != TEXT("big") && Board != TEXT("tall") && Board != TEXT("small")) Out.Add(TEXT("menu tab ") + Id + TEXT(": no board size ") + Board);
	}
	if (Colours)
		for (const auto& Cv : (*Colours)->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
			FString B, L;
			if (!Cv.Value->TryGetArray(P) || P->Num() != 2 || !(*P)[0]->TryGetString(B) || !(*P)[1]->TryGetString(L) || !IsHex(B) || !IsHex(L))
				Out.Add(TEXT("menu colour ") + K(Cv.Key) + TEXT(": needs a board colour and a lettering colour, as #rrggbb"));
		}
	if (!Colours || !(*Colours)->HasField(TEXT("quiet"))) Out.Add(TEXT("menu colours: no quiet colour for the small boards"));
	const TSharedPtr<FJsonObject>* LL = nullptr;   // ADR-0049: the cards' colours, as #rrggbb
	M->TryGetObjectField(TEXT("lamplight"), LL);
	for (const TCHAR* F : { TEXT("stone"), TEXT("stone_deep"), TEXT("stone_lift"), TEXT("cream"), TEXT("dim"), TEXT("ink"), TEXT("lantern"), TEXT("paper"), TEXT("short_tag"), TEXT("good"), TEXT("bad"), TEXT("veil") })
	{
		FString Hex;
		if (!LL || !(*LL)->TryGetStringField(F, Hex) || !IsHex(Hex)) Out.Add(FString(TEXT("menu lamplight.")) + F + TEXT(": needs a colour as #rrggbb"));
	}
	{
		const TSharedPtr<FJsonValue> Va = LL ? (*LL)->TryGetField(TEXT("veil_alpha")) : nullptr;
		if (!IsNum(Va) || Va->AsNumber() <= 0 || Va->AsNumber() >= 1) Out.Add(TEXT("menu lamplight.veil_alpha: needs a number between 0 and 1"));
	}
	const TSharedPtr<FJsonObject>* Live = nullptr;
	M->TryGetObjectField(TEXT("live"), Live);
	for (const TCHAR* F : { TEXT("face_d"), TEXT("face_y"), TEXT("face_m"), TEXT("body_y"), TEXT("body_m"), TEXT("body_fill") })
	{
		const TSharedPtr<FJsonValue> V = Live ? (*Live)->TryGetField(F) : nullptr;
		if (!IsNum(V) || V->AsNumber() <= 0) Out.Add(FString(TEXT("menu live.")) + F + TEXT(": needs a number above 0"));
	}
	if (Live) { const TSharedPtr<FJsonValue> V = (*Live)->TryGetField(TEXT("body_fill")); if (IsNum(V) && V->AsNumber() > 1) Out.Add(TEXT("menu live.body_fill: the player cannot fill more than the screen")); }
	// every platform binds every action, and no control does two things where both can happen (UI-008, UI-010)
	for (const auto& P : S->GetObjectField(TEXT("platforms"))->Values)
	{
		const TSharedPtr<FJsonObject> Got = P.Value->AsObject()->GetObjectField(TEXT("controls"));
		const FString Pn = K(P.Key);
		for (const auto& X : A->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>* L = nullptr;
			if (!Got->TryGetArrayField(*K(X.Key), L) || !L->Num()) Out.Add(Pn + TEXT(": nothing does ") + K(X.Key));
		}
		TMap<FString, TArray<FString>> By;
		for (const auto& X : Got->Values)
		{
			if (!A->HasField(K(X.Key))) { Out.Add(Pn + TEXT(": ") + K(X.Key) + TEXT(" is not an action")); continue; }
			for (const auto& Key : X.Value->AsArray()) By.FindOrAdd(Key->AsString()).Add(K(X.Key));
		}
		TArray<FString> Keys;
		By.GetKeys(Keys);
		Keys.Sort([](const FString& L, const FString& R) { return FCString::Strcmp(*L, *R) < 0; });
		for (const FString& Key : Keys)
		{
			TArray<FString> Ctx;
			for (const FString& Act : By[Key]) Ctx.Add(A->GetObjectField(Act)->GetStringField(TEXT("context")));
			TSet<FString> Unique(Ctx);
			if (Unique.Num() != Ctx.Num() || (Ctx.Contains(TEXT("anywhere")) && Ctx.Num() > 1)) Out.Add(Pn + TEXT(": ") + Key + TEXT(" does ") + FString::Join(By[Key], TEXT(" and ")));
		}
	}
	if (S->GetObjectField(TEXT("save"))->GetNumberField(TEXT("slots")) < 1) Out.Add(TEXT("save: at least one slot"));
	return Out;
}

bool ParseBrixSettings(const TSharedPtr<FJsonObject>& J, FBrixSettings& Out, FString& OutError)
{
	Out = FBrixSettings();
	Out.Json = J;
	const TArray<FString> Bad = BrixSettingsProblems(J);
	if (Bad.Num()) { OutError = Bad[0]; return false; }
	for (const auto& G : J->GetObjectField(TEXT("groups"))->Values)
	{
		const TSharedPtr<FJsonObject> Go = G.Value->AsObject();
		Out.Groups.Add({ K(G.Key), Go->GetStringField(TEXT("name")) });
		for (const auto& Sv : Go->GetObjectField(TEXT("settings"))->Values)
		{
			const TSharedPtr<FJsonObject> O = Sv.Value->AsObject();
			FBrixSetting& X = Out.Settings.AddDefaulted_GetRef();
			X.Group = K(G.Key); X.Key = K(Sv.Key); X.Name = O->GetStringField(TEXT("name")); X.Type = O->GetStringField(TEXT("type"));
			if (X.Type == TEXT("switch")) X.Default = O->GetBoolField(TEXT("default")) ? 1 : 0;
			else if (X.Type == TEXT("choice"))
			{
				X.DefaultChoice = O->GetStringField(TEXT("default"));
				for (const auto& Op : O->GetObjectField(TEXT("options"))->Values) X.Options.Add({ K(Op.Key), Op.Value->AsString() });
			}
			else { X.Min = O->GetNumberField(TEXT("min")); X.Max = O->GetNumberField(TEXT("max")); X.Step = O->GetNumberField(TEXT("step")); X.Default = O->GetNumberField(TEXT("default")); }
			X.Value = X.Default;
			X.Choice = X.DefaultChoice;
		}
	}
	const TSharedPtr<FJsonObject> M = J->GetObjectField(TEXT("menu"));
	M->TryGetBoolField(TEXT("pauses"), Out.bPauses);
	for (const auto& G : M->GetArrayField(TEXT("groups"))) Out.MenuGroups.Add({ G->AsObject()->GetStringField(TEXT("id")), G->AsObject()->GetStringField(TEXT("name")) });
	for (const auto& Cv : M->GetObjectField(TEXT("colours"))->Values)
		Out.Colours.Add(K(Cv.Key), { FColor::FromHex(Cv.Value->AsArray()[0]->AsString()), FColor::FromHex(Cv.Value->AsArray()[1]->AsString()) });
	for (const auto& Lv : M->GetObjectField(TEXT("lamplight"))->Values)
	{
		FString C;
		if (Lv.Value->TryGetString(C) && C.StartsWith(TEXT("#"))) Out.Lamplight.Add(K(Lv.Key), FColor::FromHex(C));
	}
	if (FColor* V = Out.Lamplight.Find(TEXT("veil"))) V->A = (uint8)FMath::RoundToInt(255 * M->GetObjectField(TEXT("lamplight"))->GetNumberField(TEXT("veil_alpha")));
	for (const auto& Tv : M->GetArrayField(TEXT("tabs")))
	{
		const TSharedPtr<FJsonObject> T = Tv->AsObject();
		FBrixMenuTab& Tab = Out.Tabs.AddDefaulted_GetRef();
		Tab.Id = T->GetStringField(TEXT("id")); Tab.Name = T->GetStringField(TEXT("name")); Tab.Action = T->GetStringField(TEXT("action"));
		Tab.Group = T->GetStringField(TEXT("group")); Tab.Colour = T->GetStringField(TEXT("colour")); T->TryGetStringField(TEXT("board"), Tab.Board);
	}
	for (const auto& C : J->GetObjectField(TEXT("contexts"))->Values) Out.Contexts.Add({ K(C.Key), C.Value->AsString() });
	for (const auto& A : J->GetObjectField(TEXT("actions"))->Values)
	{
		Out.Actions.Add({ K(A.Key), A.Value->AsObject()->GetStringField(TEXT("name")) });
		Out.ActionContext.Add(K(A.Key), A.Value->AsObject()->GetStringField(TEXT("context")));
	}
	for (const auto& P : J->GetObjectField(TEXT("platforms"))->Values)
	{
		Out.PlatformNames.Add(K(P.Key), P.Value->AsObject()->GetStringField(TEXT("name")));
		TMap<FString, TArray<FString>>& C = Out.Platforms.Add(K(P.Key));
		for (const auto& X : P.Value->AsObject()->GetObjectField(TEXT("controls"))->Values)
		{
			TArray<FString>& L = C.Add(K(X.Key));
			for (const auto& V : X.Value->AsArray()) L.Add(V->AsString());
		}
	}
	Out.SaveSlots = (int32)J->GetObjectField(TEXT("save"))->GetNumberField(TEXT("slots"));
	return true;
}

bool LoadBrixSettings(const FString& Path, FBrixSettings& Out, FString& OutError)
{
	FString Text;
	TSharedPtr<FJsonObject> J;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J) { OutError = TEXT("cannot read ") + Path; return false; }
	return ParseBrixSettings(J, Out, OutError);
}

FBrixSetting* FBrixSettings::Find(const FString& G, const FString& Key) { return Settings.FindByPredicate([&](const FBrixSetting& S) { return S.Group == G && S.Key == Key; }); }
const FBrixSetting* FBrixSettings::Find(const FString& G, const FString& Key) const { return Settings.FindByPredicate([&](const FBrixSetting& S) { return S.Group == G && S.Key == Key; }); }
double FBrixSettings::Number(const FString& G, const FString& Key) const { const FBrixSetting* S = Find(G, Key); return S ? S->Value : 0; }

void FBrixSettings::Step(const FString& G, const FString& Key, int32 Steps)
{
	FBrixSetting* S = Find(G, Key);
	if (!S) return;
	if (S->Type == TEXT("switch")) S->Value = S->Value > .5 ? 0 : 1;
	else if (S->Type == TEXT("choice") && S->Options.Num())
	{
		const int32 At = S->Options.IndexOfByPredicate([&](const TPair<FString, FString>& O) { return O.Key == S->Choice; });
		S->Choice = S->Options[((At + Steps) % S->Options.Num() + S->Options.Num()) % S->Options.Num()].Key;
	}
	else S->Value = FMath::Clamp(S->Value + Steps * S->Step, S->Min, S->Max);
}

void FBrixSettings::Reset() { for (FBrixSetting& S : Settings) { S.Value = S.Default; S.Choice = S.DefaultChoice; } }

FString FBrixSettings::Save() const
{
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	for (const FBrixSetting& S : Settings)
	{
		TSharedPtr<FJsonObject> G = O->HasField(S.Group) ? O->GetObjectField(S.Group) : MakeShared<FJsonObject>();
		if (S.Type == TEXT("switch")) G->SetBoolField(S.Key, S.Value > .5);
		else if (S.Type == TEXT("choice")) G->SetStringField(S.Key, S.Choice);
		else G->SetNumberField(S.Key, S.Value);
		O->SetObjectField(S.Group, G);
	}
	FString Text;
	FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Text));
	return Text;
}

void FBrixSettings::Load(const FString& Text)
{
	TSharedPtr<FJsonObject> O;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) || !O) return;
	for (FBrixSetting& S : Settings)   // the same check as life.js okSetting(): a value that does not fit is left at its default
	{
		const TSharedPtr<FJsonObject>* G = nullptr;
		if (!O->TryGetObjectField(S.Group, G)) continue;
		const TSharedPtr<FJsonValue> V = (*G)->TryGetField(S.Key);
		if (!V) continue;
		if (S.Type == TEXT("switch") && V->Type == EJson::Boolean) S.Value = V->AsBool() ? 1 : 0;
		else if (S.Type == TEXT("choice") && V->Type == EJson::String && S.Options.ContainsByPredicate([&](const TPair<FString, FString>& P) { return P.Key == V->AsString(); })) S.Choice = V->AsString();
		else if (S.Type == TEXT("range") && V->Type == EJson::Number && V->AsNumber() >= S.Min - 1e-9 && V->AsNumber() <= S.Max + 1e-9) S.Value = V->AsNumber();
	}
}
