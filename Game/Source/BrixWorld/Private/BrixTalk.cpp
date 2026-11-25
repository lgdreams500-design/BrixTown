#include "BrixTalk.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ReadJsonAt(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("Cannot read ") + Path; return false; }
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) || !Out.IsValid()) { OutError = Path + TEXT(" is not valid JSON"); return false; }
		return true;
	}
	double NumOr(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double D) { double V = D; if (O.IsValid()) O->TryGetNumberField(K, V); return V; }
	void Pair(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double Out[2])
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O->TryGetArrayField(K, A) && A->Num() >= 2) { Out[0] = (*A)[0]->AsNumber(); Out[1] = (*A)[1]->AsNumber(); }
	}
	FBrixFace::FMouth MouthOf(const TSharedPtr<FJsonObject>& M)
	{
		FBrixFace::FMouth Mo;
		Mo.Width = NumOr(M, TEXT("width"), 0); Mo.Opening = NumOr(M, TEXT("opening"), 0); Mo.Round = NumOr(M, TEXT("round"), 0);
		Mo.Teeth = (int32)NumOr(M, TEXT("teeth"), 0); M->TryGetBoolField(TEXT("tongue"), Mo.bTongue); Mo.Curve = NumOr(M, TEXT("curve"), 0);
		return Mo;
	}
	TMap<FString, FString> StringMap(const TSharedPtr<FJsonObject>& O)
	{
		TMap<FString, FString> M;
		if (O.IsValid()) for (const auto& KV : O->Values) { FString V; if (KV.Value->TryGetString(V)) M.Add(FString(KV.Key), V); }
		return M;
	}
	TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		const TSharedPtr<FJsonObject>* P = nullptr;
		return O.IsValid() && O->TryGetObjectField(K, P) ? *P : TSharedPtr<FJsonObject>();
	}
}

bool LoadBrixTalk(const FString& RulesDir, const FString& TalkPath, FBrixSpeech& S, FBrixDialogue& D, FString& OutError)
{
	S = FBrixSpeech();
	D = FBrixDialogue();
	TSharedPtr<FJsonObject> Sp, Dl, Tk;
	if (!ReadJsonAt(FPaths::Combine(RulesDir, TEXT("speech.json")), Sp, OutError) || !ReadJsonAt(FPaths::Combine(RulesDir, TEXT("dialogue.json")), Dl, OutError)
		|| !ReadJsonAt(TalkPath, Tk, OutError)) return false;
	// ---- speech.json
	const TSharedPtr<FJsonObject> Draw = Obj(Sp, TEXT("draw"));
	S.DrawWidth = NumOr(Draw, TEXT("width"), 1); S.DrawOpening = NumOr(Draw, TEXT("opening"), 1);
	if (const TSharedPtr<FJsonObject> Sh = Obj(Sp, TEXT("shapes"))) for (const auto& KV : Sh->Values) S.Shapes.Add(FString(KV.Key), MouthOf(KV.Value->AsObject()));
	if (const TSharedPtr<FJsonObject> Ex = Obj(Sp, TEXT("expressions")))
		for (const auto& KV : Ex->Values)
		{
			const TSharedPtr<FJsonObject> E = KV.Value->AsObject();
			FBrixSpeech::FExpression X;
			FString Name;
			if (E->TryGetStringField(TEXT("mouth"), Name)) X.MouthShape = Name;
			else if (const TSharedPtr<FJsonObject> M = Obj(E, TEXT("mouth"))) X.Mouth = MouthOf(M);
			X.Curve = NumOr(E, TEXT("curve"), 0); X.Lid = NumOr(E, TEXT("lid"), 0); X.Squint = NumOr(E, TEXT("squint"), 0); X.Open = NumOr(E, TEXT("open"), 1);
			Pair(E, TEXT("brow_l"), X.BrowL); Pair(E, TEXT("brow_r"), X.BrowR); Pair(E, TEXT("gaze"), X.Gaze);
			S.Expressions.Add(FString(KV.Key), X);
		}
	const TSharedPtr<FJsonObject> T = Obj(Sp, TEXT("timing"));
	S.Fps = NumOr(T, TEXT("fps"), 12);
	const TSharedPtr<FJsonObject> Fr = Obj(T, TEXT("frames"));
	S.FrameQuick = NumOr(Fr, TEXT("quick"), 1); S.FrameNormal = NumOr(Fr, TEXT("normal"), 2); S.FrameStressed = NumOr(Fr, TEXT("stressed"), 3);
	if (T.IsValid()) T->TryGetStringArrayField(TEXT("stressed_shapes"), S.StressedShapes);
	S.StressedWordLetters = (int32)NumOr(T, TEXT("stressed_word_letters"), 4);
	if (const TSharedPtr<FJsonObject> P = Obj(T, TEXT("pauses"))) for (const auto& KV : P->Values) S.Pauses.Add(FString(KV.Key), KV.Value->AsNumber());
	S.EndShape = StringMap(Obj(T, TEXT("end")));
	const TSharedPtr<FJsonObject> Bl = Obj(T, TEXT("blend"));
	S.BlendMouth = NumOr(Bl, TEXT("mouth"), 18); S.BlendExpression = NumOr(Bl, TEXT("expression"), 5);
	const TSharedPtr<FJsonObject> X = Obj(Sp, TEXT("text"));
	if (X.IsValid()) X->TryGetStringField(TEXT("vowels"), S.Vowels);
	S.Digraphs = StringMap(Obj(X, TEXT("digraphs")));
	S.Letters = StringMap(Obj(X, TEXT("letters")));   // "h": null has no shape and is left out
	const TSharedPtr<FJsonObject> A = Obj(Sp, TEXT("accents"));
	S.LineStartBrow = NumOr(A, TEXT("line_start_brow"), 0); S.LineStartSeconds = NumOr(A, TEXT("line_start_seconds"), 1);
	S.QuestionBrow = NumOr(A, TEXT("question_brow"), 0); S.QuestionSeconds = NumOr(A, TEXT("question_seconds"), 1);
	S.Nod = NumOr(A, TEXT("nod"), 0); S.NodSeconds = NumOr(A, TEXT("nod_seconds"), 1);
	S.AccentWordLetters = (int32)NumOr(A, TEXT("stressed_word_letters"), 6);

	// ---- dialogue.json
	if (const TSharedPtr<FJsonObject> Cs = Obj(Dl, TEXT("conversations")))
		for (const auto& KV : Cs->Values)
		{
			const TSharedPtr<FJsonObject> C = KV.Value->AsObject();
			FBrixDialogue::FConversation& Cv = D.Conversations.Add(FString(KV.Key));
			C->TryGetStringField(TEXT("title"), Cv.Title);
			C->TryGetStringField(TEXT("start"), Cv.Start);
			Cv.Vars = StringMap(Obj(C, TEXT("vars")));
			if (const TSharedPtr<FJsonObject> Ns = Obj(C, TEXT("nodes")))
				for (const auto& NV : Ns->Values)
				{
					const TSharedPtr<FJsonObject> N = NV.Value->AsObject();
					FBrixDialogue::FNode& Node = Cv.Nodes.Add(FString(NV.Key));
					N->TryGetStringField(TEXT("say"), Node.Say);
					N->TryGetStringField(TEXT("mood"), Node.Mood);
					const TArray<TSharedPtr<FJsonValue>>* Os = nullptr;
					if (N->TryGetArrayField(TEXT("options"), Os))
						for (const TSharedPtr<FJsonValue>& OV : *Os)
						{
							const TSharedPtr<FJsonObject> O = OV->AsObject();
							FBrixDialogue::FOption& Op = Node.Options.AddDefaulted_GetRef();
							O->TryGetStringField(TEXT("text"), Op.Text);
							O->TryGetStringField(TEXT("next"), Op.Next);   // null: the talk ends (stays empty)
							O->TryGetStringArrayField(TEXT("if"), Op.If);
							O->TryGetStringArrayField(TEXT("do"), Op.Do);
						}
				}
		}
	const TArray<TSharedPtr<FJsonValue>>* Rs = nullptr;
	if (Dl->TryGetArrayField(TEXT("rules"), Rs))
		for (const TSharedPtr<FJsonValue>& RV : *Rs)
		{
			const TSharedPtr<FJsonObject> R = RV->AsObject();
			FBrixDialogue::FRule& Rule = D.Rules.AddDefaulted_GetRef();
			R->TryGetStringField(TEXT("conversation"), Rule.Conversation);
			R->TryGetStringArrayField(TEXT("roles"), Rule.Roles);
			R->TryGetStringArrayField(TEXT("acts"), Rule.Acts);
			bool W = false;
			if (R->TryGetBoolField(TEXT("walking"), W)) Rule.bWalking = W;
		}
	D.Residents = StringMap(Obj(Dl, TEXT("residents")));
	D.RoleWords = StringMap(Obj(Dl, TEXT("role_words")));

	// ---- talk.json
	D.Doing = StringMap(Obj(Tk, TEXT("doing")));
	D.Going = StringMap(Obj(Tk, TEXT("going")));
	Tk->TryGetStringField(TEXT("player_name"), D.PlayerName);
	Tk->TryGetStringField(TEXT("voice_dir"), D.VoiceDir);
	if (const TSharedPtr<FJsonObject> V = Obj(Tk, TEXT("voices")))
	{
		V->TryGetStringArrayField(TEXT("configs"), D.VoiceConfigs);
		const TArray<TSharedPtr<FJsonValue>>* Res = nullptr;
		if (V->TryGetArrayField(TEXT("residents"), Res)) for (const TSharedPtr<FJsonValue>& R : *Res) D.ResidentVoice.Add((int32)R->AsNumber());
		if (const TSharedPtr<FJsonObject> P = Obj(V, TEXT("player"))) for (const auto& KV : P->Values) D.PlayerVoices.Add(FString(KV.Key), (int32)KV.Value->AsNumber());
		V->TryGetStringField(TEXT("player_default"), D.PlayerVoice);
		const TSharedPtr<FJsonObject> Pl = Obj(V, TEXT("play"));
		D.GapS = NumOr(Pl, TEXT("gap_s"), .06); D.BlankGapS = NumOr(Pl, TEXT("blank_gap_s"), .35); D.Volume = NumOr(Pl, TEXT("volume"), .9);
		if (const TSharedPtr<FJsonObject> C = Obj(V, TEXT("clips")))
			for (const auto& KV : C->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>& F = KV.Value->AsArray();
				if (F.Num() >= 2) D.Clips.Add(FString(KV.Key), TPair<FString, double>(F[0]->AsString(), F[1]->AsNumber()));
			}
	}
	if (!D.Conversations.Num() || !S.Shapes.Num()) { OutError = TEXT("no conversations or no mouth shapes"); return false; }
	return true;
}

// ---- the mouth and face (conversations.js speak(), SHAPE(), faceTarget(), blendFace(), restFace())

FBrixLine FBrixSpeech::Speak(const FString& Text) const
{
	FBrixLine L;
	double t = 0;
	auto Put = [&](const FString& Shape, double d)
	{
		if (L.Seq.Num() && L.Seq.Last().Shape == Shape) L.Seq.Last().D += d;
		else L.Seq.Add({ Shape, t, d });
		t += d;
	};
	const FString Low = Text.ToLower();
	static const FString Punct = TEXT(".,!?;:—");
	int32 i = 0;
	while (i < Low.Len())   // the slice's /([a-z']+)|([.,!?;:—])/g
	{
		const TCHAR c = Low[i];
		if ((c >= 'a' && c <= 'z') || c == '\'')
		{
			int32 j = i;
			while (j < Low.Len() && ((Low[j] >= 'a' && Low[j] <= 'z') || Low[j] == '\'')) ++j;
			const FString W = Low.Mid(i, j - i).Replace(TEXT("'"), TEXT(""));
			i = j;
			const bool bStressed = W.Len() >= StressedWordLetters;
			L.Words.Add({ t, W.Len() >= AccentWordLetters });
			for (int32 k = 0; k < W.Len();)
			{
				const FString Two = W.Mid(k, 2);
				const FString* Di = Digraphs.Find(Two);
				const FString Ch = Di ? Two.Left(1) : W.Mid(k, 1);
				const FString* Sh = Di ? Di : Letters.Find(Ch);
				k += Di ? 2 : 1;
				if (!Sh) continue;
				const bool bVowel = Vowels.Contains(Ch, ESearchCase::CaseSensitive);
				Put(*Sh, (bVowel ? (bStressed && StressedShapes.Contains(*Sh) ? FrameStressed : FrameNormal) : FrameQuick) / Fps);
			}
			continue;
		}
		int32 Pi = INDEX_NONE;
		if (Punct.FindChar(c, Pi))
		{
			const double* P = Pauses.Find(FString::Chr(c));
			Put(TEXT("rest"), P ? *P : 0);
			if (c == '?') L.Asks.Add(t);
		}
		++i;
	}
	L.End = t;
	return L;
}

FBrixFace::FMouth FBrixSpeech::Shape(const FString& Name) const
{
	const FBrixFace::FMouth* M = Shapes.Find(Name);
	FBrixFace::FMouth O = M ? *M : FBrixFace::FMouth();
	O.Width *= DrawWidth;
	O.Opening *= DrawOpening;
	return O;
}

FBrixFace FBrixSpeech::Target(const FBrixLine& Line, const FString& Mood, double t) const
{
	const FExpression* E = Expressions.Find(Mood);
	if (!E) E = Expressions.Find(TEXT("neutral"));
	static const FExpression None;
	if (!E) E = &None;
	FBrixFace::FMouth Mouth;
	if (t < Line.End && Line.Seq.Num())
	{
		int32 k = Line.Seq.IndexOfByPredicate([&](const FBrixLine::FShape& q) { return t < q.T0 + q.D; });
		Mouth = Shape(Line.Seq[k < 0 ? Line.Seq.Num() - 1 : k].Shape);
	}
	else if (Mood == TEXT("neutral") || Mood == TEXT("happy"))
	{
		const FString* End = EndShape.Find(Mood);
		if (!End) End = EndShape.Find(TEXT("default"));
		Mouth = Shape(End ? *End : FString(TEXT("rest")));
	}
	else if (E->MouthShape) Mouth = Shape(*E->MouthShape);
	else { Mouth = E->Mouth; Mouth.Width *= DrawWidth; Mouth.Opening *= DrawOpening; }
	double Lift = t < LineStartSeconds ? LineStartBrow * (1 - t / LineStartSeconds) : 0, NodNow = 0;
	for (const double q : Line.Asks) if (t > q - QuestionSeconds && t < q) Lift = FMath::Max(Lift, QuestionBrow);
	for (const FBrixLine::FWord& w : Line.Words)
		if (w.bLong && t >= w.T0 && t < w.T0 + NodSeconds)
		{
			const double u = (t - w.T0) / NodSeconds;
			NodNow = Nod * FMath::Sin(UE_DOUBLE_PI * u);
			Lift = FMath::Max(Lift, LineStartBrow * FMath::Sin(UE_DOUBLE_PI * u));
		}
	FBrixFace F;
	F.Mouth = Mouth;
	F.Curve = E->Curve + Mouth.Curve * .5;
	F.BrowL[0] = E->BrowL[0] + Lift; F.BrowL[1] = E->BrowL[1] + Lift;
	F.BrowR[0] = E->BrowR[0] + Lift; F.BrowR[1] = E->BrowR[1] + Lift;
	F.Lid = E->Lid; F.Squint = E->Squint; F.Open = E->Open;
	F.Gaze[0] = E->Gaze[0]; F.Gaze[1] = E->Gaze[1];
	F.Nod = NodNow;
	return F;
}

void FBrixSpeech::Blend(FBrixFace& f, const FBrixFace& g, double Dt) const
{
	const double km = FMath::Min(1.0, Dt * BlendMouth), ke = FMath::Min(1.0, Dt * BlendExpression);
	auto Mix = [](double a, double b, double k) { return a + (b - a) * k; };
	f.Mouth.Width = Mix(f.Mouth.Width, g.Mouth.Width, km);
	f.Mouth.Opening = Mix(f.Mouth.Opening, g.Mouth.Opening, km);
	f.Mouth.Round = Mix(f.Mouth.Round, g.Mouth.Round, km);
	f.Mouth.Teeth = g.Mouth.Teeth;
	f.Mouth.bTongue = g.Mouth.bTongue;
	f.Curve = Mix(f.Curve, g.Curve, ke); f.Lid = Mix(f.Lid, g.Lid, ke); f.Squint = Mix(f.Squint, g.Squint, ke); f.Open = Mix(f.Open, g.Open, ke);
	f.Nod = Mix(f.Nod, g.Nod, km);
	for (int32 j = 0; j < 2; ++j) { f.BrowL[j] = Mix(f.BrowL[j], g.BrowL[j], ke); f.BrowR[j] = Mix(f.BrowR[j], g.BrowR[j], ke); f.Gaze[j] = Mix(f.Gaze[j], g.Gaze[j], ke); }
}

FBrixFace FBrixSpeech::Rest() const
{
	FBrixFace F;
	F.Mouth = Shape(TEXT("rest"));
	F.Curve = .075;
	return F;
}

// ---- the conversations (conversations.js conversationFor(), fill(), has()) and the voices (voices.js plan())

FString FBrixDialogue::ConversationFor(int32 Resident, const FString& Role, const FString& Act, bool bWalking) const
{
	if (const FString* Own = Residents.Find(FString::FromInt(Resident))) return *Own;
	for (const FRule& R : Rules)
	{
		if (R.Roles.Num() && !R.Roles.Contains(Role)) continue;
		if (R.Acts.Num() && !R.Acts.Contains(Act)) continue;
		if (R.bWalking && *R.bWalking != bWalking) continue;
		return R.Conversation;
	}
	return FString();
}

FString FBrixDialogue::Fill(const FString& Text, const TMap<FString, FString>& Vars)
{
	FString Out;
	int32 i = 0;
	while (i < Text.Len())
	{
		if (Text[i] == '{')
		{
			int32 j = i + 1;
			while (j < Text.Len() && (FChar::IsAlnum(Text[j]) || Text[j] == '_')) ++j;
			if (j < Text.Len() && Text[j] == '}' && j > i + 1)
			{
				const FString Key = Text.Mid(i + 1, j - i - 1);
				const FString* V = Vars.Find(Key);
				Out += V ? *V : Text.Mid(i, j - i + 1);
				i = j + 1;
				continue;
			}
		}
		Out.AppendChar(Text[i++]);
	}
	return Out;
}

bool FBrixDialogue::Allowed(const FOption& O, const TSet<FString>& Flags)
{
	for (const FString& X : O.If)
		if (X.StartsWith(TEXT("!")) ? Flags.Contains(X.Mid(1)) : !Flags.Contains(X)) return false;
	return true;
}

TOptional<FBrixDialogue::FPlan> FBrixDialogue::Plan(int32 Who, const FString& Template, const TMap<FString, FString>& Vars, const FString& Mood) const
{
	int32 Cfg = -1;
	if (Who < 0) { const int32* P = PlayerVoices.Find(PlayerVoice); Cfg = P ? *P : -1; }
	else if (ResidentVoice.IsValidIndex(Who)) Cfg = ResidentVoice[Who];
	if (!VoiceConfigs.IsValidIndex(Cfg)) return {};
	const FString& C = VoiceConfigs[Cfg];
	// a piece as it is looked up: no leading punctuation or spaces, single spaces (voices.js norm())
	auto Norm = [](const FString& T)
	{
		int32 s = 0;
		static const FString Lead = TEXT(" \t\r\n,;:.!?—-");
		int32 Dummy;
		while (s < T.Len() && Lead.FindChar(T[s], Dummy)) ++s;
		FString O;
		bool bSpace = false;
		for (int32 k = s; k < T.Len(); ++k)
		{
			if (FChar::IsWhitespace(T[k])) { bSpace = true; continue; }
			if (bSpace && O.Len()) O.AppendChar(' ');
			bSpace = false;
			O.AppendChar(T[k]);
		}
		return O;
	};
	auto Speakable = [](const FString& T) { for (TCHAR c : T) if (FChar::IsAlnum(c) && c < 128) return true; return false; };
	FPlan P;
	int32 Found = 0;
	int32 i = 0;
	while (i <= Template.Len())   // the template split at its {blanks}
	{
		int32 Open = Template.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i), Close = INDEX_NONE;
		bool bVar = false;
		if (Open != INDEX_NONE)
		{
			Close = Template.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open);
			bVar = Close != INDEX_NONE;
			for (int32 k = Open + 1; bVar && k < Close; ++k) if (!(FChar::IsAlnum(Template[k]) || Template[k] == '_')) bVar = false;
			if (Close == Open + 1) bVar = false;
		}
		const int32 TextEnd = bVar ? Open : Template.Len();
		TArray<FString> Toks = { Template.Mid(i, TextEnd - i) };
		if (bVar) { const FString* V = Vars.Find(Template.Mid(Open + 1, Close - Open - 1)); Toks.Add(V ? *V : FString()); }
		for (const FString& Tok : Toks)
		{
			const FString p = Norm(Tok);
			if (!Speakable(p)) continue;
			const TPair<FString, double>* Clip = Clips.Find(C + TEXT("|") + Mood + TEXT("|") + p);
			if (!Clip) Clip = Clips.Find(C + TEXT("|neutral|") + p);
			if (Clip) { P.Parts.Add(*Clip); ++Found; }
			else P.Parts.Add(TPair<FString, double>(FString(), BlankGapS));
		}
		if (!bVar) break;
		i = Close + 1;
	}
	if (!Found) return {};
	for (const TPair<FString, double>& Pt : P.Parts) P.Dur += Pt.Value;
	P.Dur += GapS * (P.Parts.Num() - 1);
	return P;
}
