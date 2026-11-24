#include "BrixBody.h"

#include "BrixDay.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// A line-for-line port of Prototype/play/people.js (its names are kept, so the two can be read side by side). Left out:
// what only the slice needs (its own self-shadowing and texture marks, which Unreal's light and materials do), and the
// poses residents out of doors do not take yet here (sitting, lying, riding a board, umbrellas, hoods, talking faces).

// ---- the tables (Game/Data/people.json)

const FBrixSignature& FBrixPeopleRules::Sig(const FString& Name) const
{
	if (const FBrixSignature* S = Signatures.Find(Name)) return *S;
	if (const FBrixSignature* S = Signatures.Find(TEXT("Easy"))) return *S;   // people.js: SIG[name] || SIG.Easy
	static const FBrixSignature Base;
	return Base;
}

namespace
{
	FVector3f Vec3(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		return A.Num() >= 3 ? FVector3f((float)A[0]->AsNumber(), (float)A[1]->AsNumber(), (float)A[2]->AsNumber()) : FVector3f::ZeroVector;
	}
	TArray<FVector3f> Vec3List(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<FVector3f> Out;
		const TArray<TSharedPtr<FJsonValue>>* A;
		if (O->TryGetArrayField(Key, A)) for (const TSharedPtr<FJsonValue>& V : *A) Out.Add(Vec3(V->AsArray()));
		return Out;
	}
	TArray<FString> Names(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		TArray<FString> Out;
		O->TryGetStringArrayField(Key, Out);
		return Out;
	}
	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double Default = 0)
	{
		double V = Default;
		O->TryGetNumberField(Key, V);
		return V;
	}
	FVector2D Range(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, FVector2D Default)
	{
		const TArray<TSharedPtr<FJsonValue>>* A;
		if (O->TryGetArrayField(Key, A) && A->Num() >= 2) return FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber());
		return Default;
	}
}

bool LoadBrixPeopleRules(const FString& Path, FBrixPeopleRules& Out, FString& OutError)
{
	Out = FBrixPeopleRules();
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("Cannot read ") + Path; return false; }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { OutError = Path + TEXT(" is not valid JSON"); return false; }
	const TSharedPtr<FJsonObject>* Sigs;
	if (Root->TryGetObjectField(TEXT("signatures"), Sigs))
		for (const auto& KV : (*Sigs)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
			FBrixSignature S;
			S.Name = FString(KV.Key);
			S.Cycle = Num(O, TEXT("cycle"), S.Cycle); S.Stride = Num(O, TEXT("stride"), S.Stride); S.Duty = Num(O, TEXT("duty"), S.Duty);
			S.Fore = Num(O, TEXT("fore"), S.Fore); S.Bounce = Num(O, TEXT("bounce"), S.Bounce); S.Sway = Num(O, TEXT("sway"), S.Sway);
			S.HipYaw = Num(O, TEXT("hip_yaw"), S.HipYaw); S.HipRoll = Num(O, TEXT("hip_roll"), S.HipRoll); S.Counter = Num(O, TEXT("counter"), S.Counter);
			S.Lean = Num(O, TEXT("lean"), S.Lean); S.Width = Num(O, TEXT("width"), S.Width); S.ToeOut = Num(O, TEXT("toe_out"), S.ToeOut);
			S.Lift = Num(O, TEXT("lift"), S.Lift); S.Strike = Num(O, TEXT("strike"), S.Strike); S.ToeOff = Num(O, TEXT("toe_off"), S.ToeOff);
			S.ArmSwing = Num(O, TEXT("arm_swing"), S.ArmSwing); S.ArmBias = Num(O, TEXT("arm_bias"), S.ArmBias); S.ArmOut = Num(O, TEXT("arm_out"), S.ArmOut);
			S.Elbow = Num(O, TEXT("elbow"), S.Elbow); S.ElbowSwing = Num(O, TEXT("elbow_swing"), S.ElbowSwing);
			S.ArmL = Num(O, TEXT("arm_l"), S.ArmL); S.ArmR = Num(O, TEXT("arm_r"), S.ArmR);
			S.HeadPitch = Num(O, TEXT("head_pitch"), S.HeadPitch); S.HeadBob = Num(O, TEXT("head_bob"), S.HeadBob);
			S.HeadFollow = Num(O, TEXT("head_follow"), S.HeadFollow); S.HeadLag = Num(O, TEXT("head_lag"), S.HeadLag);
			Out.Signatures.Add(S.Name, S);
		}
	const TSharedPtr<FJsonObject>* Pal;
	if (Root->TryGetObjectField(TEXT("palette"), Pal))
		for (const auto& KV : (*Pal)->Values) Out.Palette.Add(FString(KV.Key), Vec3(KV.Value->AsArray()));
	Out.Skins = Vec3List(Root, TEXT("skins"));
	Out.Hairs = Vec3List(Root, TEXT("hairs"));
	Out.Eyes = Vec3List(Root, TEXT("eyes"));
	const TSharedPtr<FJsonObject>* Roles;
	if (Root->TryGetObjectField(TEXT("roles"), Roles))
		for (const auto& KV : (*Roles)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
			FBrixRoleWear R;
			O->TryGetStringField(TEXT("g"), R.Garment);
			R.Top = Names(O, TEXT("top")); R.Bottom = Names(O, TEXT("bottom")); R.Apron = Names(O, TEXT("apron")); R.Cap = Names(O, TEXT("cap"));
			R.Scarf = Names(O, TEXT("scarf")); R.Hair = Names(O, TEXT("hair")); R.Shoes = Names(O, TEXT("shoes"));
			R.Skirt = Num(O, TEXT("skirt")); R.Shorts = Num(O, TEXT("shorts")); R.Bag = Num(O, TEXT("bag")); R.ScarfP = Num(O, TEXT("scarfP")); R.Tash = Num(O, TEXT("tash"));
			R.bGrey = Num(O, TEXT("grey")) != 0;
			R.Height = Range(O, TEXT("h"), R.Height);
			R.Build = Range(O, TEXT("b"), R.Build);
			Out.Roles.Add(FString(KV.Key), R);
		}
	const TSharedPtr<FJsonObject>* Pl;
	if (Root->TryGetObjectField(TEXT("player"), Pl))   // a look as people.js writes one: sizes, colours, names
	{
		const TSharedPtr<FJsonObject>& O = *Pl;
		FBrixLook& L = Out.Player;
		auto C = [&](const TCHAR* K, FVector3f& Into) { const TArray<TSharedPtr<FJsonValue>>* A; if (O->TryGetArrayField(K, A)) Into = Vec3(*A); };
		auto Opt = [&](const TCHAR* K, TOptional<FVector3f>& Into) { const TArray<TSharedPtr<FJsonValue>>* A; if (O->TryGetArrayField(K, A)) Into = Vec3(*A); };
		L.S = Num(O, TEXT("S"), 1); L.W = Num(O, TEXT("W"), 1);
		C(TEXT("skin"), L.Skin); C(TEXT("hairC"), L.HairC); C(TEXT("eyes"), L.Eyes); C(TEXT("top"), L.Top); C(TEXT("bottom"), L.Bottom); C(TEXT("shoes"), L.Shoes);
		Opt(TEXT("apron"), L.Apron); Opt(TEXT("cap"), L.Cap); Opt(TEXT("scarf"), L.Scarf);
		O->TryGetStringField(TEXT("hair"), L.Hair); O->TryGetStringField(TEXT("g"), L.Garment); O->TryGetStringField(TEXT("legs"), L.Legs); O->TryGetStringField(TEXT("sig"), L.SigName);
		O->TryGetBoolField(TEXT("bag"), L.bBag); O->TryGetBoolField(TEXT("tash"), L.bTash);
		L.Seed = (int32)Num(O, TEXT("seed"));
	}
	if (!Out.Signatures.Num() || !Out.Roles.Num() || !Out.Skins.Num()) { OutError = Path + TEXT(" has no signatures, roles or skins"); return false; }
	return true;
}

FBrixLook BrixLookOf(const FBrixPeopleRules& Rules, int32 Id, const FString& Role, const FString& SigName)
{
	static const FBrixRoleWear None;
	const FBrixRoleWear* Found = Rules.Roles.Find(Role);
	if (!Found) Found = Rules.Roles.Find(TEXT("commuter"));
	const FBrixRoleWear& R = Found ? *Found : None;
	const FString IdS = FString::FromInt(Id);
	auto r = [&](const TCHAR* K) { return BrixSliceHash({ IdS, K }); };
	auto Index = [](double V, int32 N) { return FMath::Clamp((int32)FMath::FloorToDouble(V * N), 0, FMath::Max(N - 1, 0)); };
	auto Colour = [&](const FString& Name) { const FVector3f* C = Rules.Palette.Find(Name); return C ? *C : FVector3f(.5f); };
	auto pk = [&](const TArray<FString>& List, const TCHAR* K) { return List.Num() ? Colour(List[Index(r(K), List.Num())]) : FVector3f(.5f); };
	FBrixLook L;
	const double Height = R.Height.X + (R.Height.Y - R.Height.X) * r(TEXT("h"));
	L.S = Height / 1.72;
	L.W = (R.Build.X + (R.Build.Y - R.Build.X) * r(TEXT("b"))) * 1.04;
	L.Skin = Rules.Skins.Num() ? Rules.Skins[(int32)FMath::FloorToDouble(r(TEXT("s")) * Rules.Skins.Num())] : FVector3f(.7f);
	if (R.Hair.Num()) L.Hair = R.Hair[(int32)FMath::FloorToDouble(r(TEXT("hs")) * R.Hair.Num())];
	L.HairC = R.bGrey ? FVector3f(.5f, .5f, .51f) : Rules.Hairs.Num() ? Rules.Hairs[(int32)FMath::FloorToDouble(r(TEXT("hc")) * Rules.Hairs.Num())] : FVector3f(.1f);
	L.Eyes = Rules.Eyes.Num() ? Rules.Eyes[(int32)FMath::FloorToDouble(r(TEXT("e")) * Rules.Eyes.Num())] : FVector3f(.2f);
	L.Garment = R.Garment;
	L.Top = pk(R.Top, TEXT("t"));
	L.Bottom = pk(R.Bottom, TEXT("bt"));
	L.Legs = R.Skirt && r(TEXT("sk")) < R.Skirt ? TEXT("skirt") : R.Shorts && r(TEXT("sh")) < R.Shorts ? TEXT("shorts") : TEXT("trousers");
	L.Shoes = pk(R.Shoes, TEXT("so"));
	if (R.Apron.Num()) L.Apron = pk(R.Apron, TEXT("ap"));
	if (R.Cap.Num()) L.Cap = pk(R.Cap, TEXT("cp"));
	L.bBag = R.Bag && r(TEXT("bg")) < R.Bag;
	if (R.Scarf.Num() && r(TEXT("sf")) < (R.ScarfP ? R.ScarfP : 1.0)) L.Scarf = pk(R.Scarf, TEXT("sc"));
	L.bTash = R.Tash && r(TEXT("ta")) < R.Tash;
	L.SigName = Rules.Signatures.Contains(SigName) ? SigName : FString(TEXT("Easy"));
	L.Seed = (int32)FMath::FloorToDouble(r(TEXT("seed")) * 1000);
	return L;
}

double BrixStepLength(const FBrixSignature& Sig, double S) { return 2 * Sig.Stride * .7885 * S; }

// ---- the maths of people.js (motion_lib's axes: the character faces -Y, +Z is up, +X is the character's left)

namespace
{
	constexpr double TAU = 2 * UE_DOUBLE_PI;
	double rad(double D) { return D * UE_DOUBLE_PI / 180; }
	double cl(double V, double A, double B) { return FMath::Max(A, FMath::Min(B, V)); }
	double sm(double X) { X = cl(X, 0, 1); return X * X * (3 - 2 * X); }
	double fmod1(double X) { return FMath::Fmod(X, 1.0); }

	struct M3
	{
		double m[3][3];
		static M3 I() { return { { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } } }; }
	};
	M3 Rx(double a) { const double c = FMath::Cos(a), s = FMath::Sin(a); return { { { 1, 0, 0 }, { 0, c, -s }, { 0, s, c } } }; }
	M3 Ry(double a) { const double c = FMath::Cos(a), s = FMath::Sin(a); return { { { c, 0, s }, { 0, 1, 0 }, { -s, 0, c } } }; }
	M3 Rz(double a) { const double c = FMath::Cos(a), s = FMath::Sin(a); return { { { c, -s, 0 }, { s, c, 0 }, { 0, 0, 1 } } }; }
	M3 mm(const M3& a, const M3& b)
	{
		M3 o;
		for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) o.m[r][c] = a.m[r][0] * b.m[0][c] + a.m[r][1] * b.m[1][c] + a.m[r][2] * b.m[2][c];
		return o;
	}
	FVector mv(const M3& a, const FVector& v)
	{
		return FVector(a.m[0][0] * v.X + a.m[0][1] * v.Y + a.m[0][2] * v.Z, a.m[1][0] * v.X + a.m[1][1] * v.Y + a.m[1][2] * v.Z, a.m[2][0] * v.X + a.m[2][1] * v.Y + a.m[2][2] * v.Z);
	}
	double len(const FVector& a) { return a.Size(); }
	FVector unit(const FVector& a) { double l = a.Size(); if (l == 0) l = 1; return a / l; }
	FVector cross(const FVector& a, const FVector& b) { return FVector::CrossProduct(a, b); }
	double dot(const FVector& a, const FVector& b) { return FVector::DotProduct(a, b); }

	struct FGeo { double S, L1, L2, leg, hipx, hipdz, pelz, ankz; };
	FGeo geo(double S) { return { S, .3895 * S, .399 * S, .7885 * S, .085 * S, -.0285 * S, .8835 * S, .0665 * S }; }

	struct FFoot { FVector P; double Pitch; };
	FFoot foot(const FBrixSignature& sig, const FGeo& g, double ph, double side, double total)   // ankle target and foot pitch at phase ph, 0 = heel strike
	{
		const double D = sig.Duty, ballL = .115 * g.S, strike = rad(sig.Strike), toe = rad(sig.ToeOff), x = side * g.hipx * sig.Width;
		double u, z, pitch;
		if (ph < D)
		{
			const double s = ph / D;
			u = total * (sig.Fore - s);
			pitch = s < .16 ? strike * (1 - s / .16) : s < .58 ? 0 : -toe * sm((s - .58) / .42);
			z = g.ankz + (pitch < 0 ? ballL * FMath::Sin(-pitch) : .03 * g.S * FMath::Sin(pitch));
			if (pitch < 0) u += ballL * (1 - FMath::Cos(pitch));
		}
		else
		{
			const double s = (ph - D) / (1 - D), e = s - FMath::Sin(TAU * s) / TAU, u0 = -total * (1 - sig.Fore) + ballL * (1 - FMath::Cos(toe));
			u = u0 + (total * sig.Fore - u0) * e;
			pitch = -toe + (toe + strike) * sm(s * 1.15);
			z = g.ankz + ballL * FMath::Sin(toe) * (1 - sm(s * 2.2)) + sig.Lift * g.S * FMath::Pow(FMath::Max(0.0, FMath::Sin(UE_DOUBLE_PI * FMath::Min(1.0, s * 1.08))), 1.2)
				+ .03 * g.S * FMath::Sin(strike) * sm((s - .7) / .3);
		}
		return { FVector(x, -u, z), pitch };
	}
	struct FPelvis { FVector loc; M3 R; double yaw, roll; };
	FPelvis pelvis(const FBrixSignature& sig, const FGeo& g, double ph, double base)
	{
		const double yaw = -rad(sig.HipYaw) * FMath::Cos(TAU * ph), roll = -rad(sig.HipRoll) * FMath::Sin(TAU * (ph - .03));
		return { FVector(sig.Sway * g.S * FMath::Sin(TAU * (ph - .06)), 0, base - sig.Bounce * g.S * FMath::Cos(2 * TAU * (ph - .045))), mm(Rz(yaw), Ry(roll)), yaw, roll };
	}
	TMap<FString, double>& BaseCache() { static TMap<FString, double> C; return C; }
	FCriticalSection& BaseLock() { static FCriticalSection L; return L; }   // bodies are built on several threads at once
	double baseHeight(const FBrixSignature& sig, const FGeo& g, double total)   // hip height that lets the planted leg reach the ground through the step
	{
		const FString Key = sig.Name + TEXT("|") + FString::Printf(TEXT("%.2f"), g.S);   // people.js keys it so too (sig.name | S.toFixed(2))
		{
			FScopeLock Lock(&BaseLock());
			if (const double* Found = BaseCache().Find(Key)) return *Found;
		}
		double best = 1e9;
		const double lmax = g.leg * .9975;
		for (int32 i = 0; i < 64; ++i)
		{
			const double ph = i / 64.0;
			const FPelvis p = pelvis(sig, g, ph, 0);
			const double Sides[2][2] = { { 1, 0 }, { -1, .5 } };
			for (const auto& SO : Sides)
			{
				const double side = SO[0], q = fmod1(ph + SO[1]);
				if (q > sig.Duty * .62) continue;
				const FFoot t = foot(sig, g, q, side, total);
				const FVector hip = p.loc + mv(p.R, FVector(side * g.hipx, 0, g.hipdz));
				const double dx = t.P.X - hip.X, dy = t.P.Y - hip.Y;
				best = FMath::Min(best, t.P.Z + FMath::Sqrt(FMath::Max(lmax * lmax - dx * dx - dy * dy, 1e-6)) - hip.Z);
			}
		}
		FScopeLock Lock(&BaseLock());
		BaseCache().FindOrAdd(Key, best);   // a thread that got there first keeps its own: the same number
		return BaseCache()[Key];
	}

	struct FPose
	{
		FVector hip[2], kn[2], an[2], heel[2], toe[2], sh[2], el[2], wr[2];
		FVector pel, spine, neck, headJ;
		M3 Rp, Rt, Rh;
	};
	void leg(FPose& P, int k, const FGeo& g, const FVector& hip, const FVector& tgt, const FVector& n0, double pitch, double toeOut)   // two-bone leg: knee bends forward
	{
		const FVector v = tgt - hip;
		const double d = cl(len(v), FMath::Abs(g.L1 - g.L2) + 1e-4, (g.L1 + g.L2) * .9985);
		const FVector vh = unit(v);
		const double a = FMath::Acos(cl((g.L1 * g.L1 + d * d - g.L2 * g.L2) / (2 * g.L1 * d), -1, 1));
		const FVector n = unit(n0 - vh * dot(vh, n0)), b = cross(vh, n);
		P.hip[k] = hip;
		P.kn[k] = hip + (vh * (g.L1 * FMath::Cos(a)) + b * (g.L1 * FMath::Sin(a)));
		P.an[k] = hip + vh * d;
		const M3 Rf = mm(Rz(toeOut), Rx(-pitch));
		const FVector f = mv(Rf, FVector(0, -1, 0)), dn = mv(Rf, FVector(0, 0, -1));
		P.heel[k] = P.an[k] + (dn * (.03 * g.S) + f * (-.035 * g.S));
		P.toe[k] = P.an[k] + (dn * (.036 * g.S) + f * (.135 * g.S));
	}
	void upper(FPose& P, const FGeo& g, double W, const FVector& pel, const M3& Rp, const double chest[3], const double arms[2][3], const double head[3])
	{
		const double S = g.S, sh = (.172 * W + .016) * S;
		const M3 Rt = mm(Rp, mm(mm(Rz(chest[0]), Rx(chest[1])), Ry(chest[2])));
		P.pel = pel; P.Rp = Rp; P.Rt = Rt;
		P.spine = pel + mv(Rp, FVector(0, 0, .098 * S));
		P.neck = P.spine + mv(Rt, FVector(0, .005 * S, .3528 * S));
		for (int k = 0; k < 2; ++k)
		{
			const double sd = k ? -1 : 1;
			const double* a = arms[k];
			const M3 Ru = mm(Rt, mm(Rx(-rad(a[0])), Ry(-sd * rad(a[1])))), Rl = mm(Ru, Rx(-rad(a[2])));
			P.sh[k] = P.spine + mv(Rt, FVector(sd * sh, 0, .3381 * S));
			P.el[k] = P.sh[k] + mv(Ru, FVector(sd * .0264 * S, -.0096 * S, -.2367 * S));
			P.wr[k] = P.el[k] + mv(Rl, FVector(sd * .0286 * S, -.0104 * S, -.2564 * S));
		}
		P.Rh = mm(mm(Rz(head[0]), Rx(head[1])), Ry(head[2]));   // the head is held in space; the neck takes 38% of the difference
		P.headJ = P.neck + unit(mv(Rt, FVector(0, 0, 1)) * .62 + mv(P.Rh, FVector(0, 0, 1)) * .38) * (.064 * S);
	}
	void wander(double t, double seed, double Out[3])
	{
		Out[0] = rad(1.6) * FMath::Sin(.9 * t + seed) + rad(.9) * FMath::Sin(2.3 * t + 1.7 * seed);
		Out[1] = rad(1) * FMath::Sin(1.3 * t + 2.1 * seed);
		Out[2] = rad(1.2) * FMath::Sin(.7 * t + .6 * seed);
	}
	FPose walk(const FBrixSignature& sig, const FGeo& g, double W, double ph, const double hm[3])
	{
		FPose P;
		const double w = TAU, total = 2 * sig.Duty * sig.Stride * g.leg;
		const FPelvis p = pelvis(sig, g, ph, baseHeight(sig, g, total));
		const FVector n0 = mv(p.R, FVector(1, 0, 0));
		const double Legs[2][3] = { { 0, 1, 0 }, { 1, -1, .5 } };
		for (const auto& KO : Legs)
		{
			const int k = (int)KO[0];
			const double side = KO[1];
			const FFoot t = foot(sig, g, fmod1(ph + KO[2]), side, total);
			leg(P, k, g, p.loc + mv(p.R, FVector(side * g.hipx, 0, g.hipdz)), t.P, n0, t.Pitch, side * rad(sig.ToeOut));
		}
		const double chest[3] = { -(1 + sig.Counter) * p.yaw, rad(sig.Lean) + rad(.8) * FMath::Cos(2 * w * (ph - .10)), -.85 * p.roll + .25 * p.roll * FMath::Cos(w * ph) };
		double arms[2][3];
		for (int k = 0; k < 2; ++k)
		{
			const double q = fmod1(ph + (k ? .5 : 0)), kk = k ? sig.ArmR : sig.ArmL, sw = -FMath::Cos(w * (q - .04));
			arms[k][0] = sig.ArmBias + sig.ArmSwing * kk * sw;
			arms[k][1] = sig.ArmOut;
			arms[k][2] = sig.Elbow + sig.ElbowSwing * kk * FMath::Max(0.0, sw) + 4 * kk * FMath::Max(0.0, -sw);
		}
		const double lag = sig.HeadLag, yawB = -sig.Counter * (-rad(sig.HipYaw) * FMath::Cos(w * (ph - lag)));
		const double head[3] = { sig.HeadFollow * yawB + hm[0], rad(sig.HeadPitch) + rad(sig.HeadBob) * FMath::Cos(2 * w * (ph - .045 - lag)) + .35 * rad(sig.Lean) + hm[1], .3 * sig.HeadFollow * p.roll + hm[2] };
		upper(P, g, W, p.loc, p.R, chest, arms, head);
		return P;
	}
	FPose stand(const FBrixSignature& sig, const FGeo& g, double W, double t, const double hm[3], double seed)
	{
		FPose P;
		const double br = FMath::Sin(t * 1.3 + seed);
		const FVector pel(.006 * g.S * FMath::Sin(t * .5 + seed), 0, g.pelz - .012 * g.S + .002 * br);
		for (int k = 0; k < 2; ++k)
		{
			const double side = k ? -1 : 1;
			leg(P, k, g, pel + FVector(side * g.hipx, 0, g.hipdz), FVector(side * g.hipx * FMath::Max(.8, sig.Width), .01 * side, g.ankz), FVector(1, 0, 0), 0, side * rad(sig.ToeOut + 4));
		}
		const double arm[3] = { sig.ArmBias, sig.ArmOut * .3, sig.Elbow * .7 + 4 };
		const double arms[2][3] = { { arm[0], arm[1], arm[2] }, { arm[0], arm[1], arm[2] } };
		const double chest[3] = { 0, rad(sig.Lean * .5) + .004 * br, 0 };
		const double head[3] = { hm[0], rad(sig.HeadPitch * .5) + hm[1], hm[2] };
		upper(P, g, W, pel, M3::I(), chest, arms, head);
		return P;
	}

	// ---- shapes: lofted rings with smooth normals, so forms read as soft
	struct FRing { FVector c, u, v; };
	using FKeep = TFunctionRef<bool(double, double, double)>;

	struct FShapes
	{
		FBrixBodyMesh& Out;
		TArray<FVector> P, N;   // the rings' points and normals (people.js scratch)

		void loft(const TArray<FRing>& rings, int32 n, const FVector3f& col, const FVector* ctr, TFunctionRef<bool(int32, int32)> mask, bool bMask)
		{
			const int32 m = rings.Num();
			P.SetNumUninitialized(m * n, EAllowShrinking::No);
			N.SetNumUninitialized(m * n, EAllowShrinking::No);
			for (int32 k = 0; k < m; ++k)
				for (int32 i = 0; i < n; ++i)
				{
					const double a = TAU * i / n, ca = FMath::Cos(a), sa = FMath::Sin(a);
					const FRing& R = rings[k];
					const FVector p = R.c + R.u * ca + R.v * sa;
					P[k * n + i] = p;
					N[k * n + i] = unit(p - (ctr ? *ctr : R.c));
				}
			// true surface normals, from the shape itself (people.js TRUEN)
			for (int32 k = 0; k < m; ++k)
			{
				const int32 k0 = FMath::Max(0, k - 1), k1 = FMath::Min(m - 1, k + 1);
				for (int32 i = 0; i < n; ++i)
				{
					const FVector A = P[k * n + (i + 1) % n] - P[k * n + (i + n - 1) % n], B = P[k1 * n + i] - P[k0 * n + i];
					const FVector nn = cross(A, B);
					const double l = len(nn);
					if (l < 1e-10) continue;
					N[k * n + i] = nn * (dot(nn, N[k * n + i]) < 0 ? -1 / l : 1 / l);
				}
			}
			const int32 First = Out.Pos.Num();
			for (int32 o = 0; o < m * n; ++o) { Out.Pos.Add(FVector3f(P[o])); Out.Normal.Add(FVector3f(N[o])); Out.Colour.Add(col); }
			for (int32 k = 0; k < m - 1; ++k)
				for (int32 i = 0; i < n; ++i)
				{
					if (bMask && !mask(k, i)) continue;
					const int32 j = (i + 1) % n, Q[6] = { k * n + i, k * n + j, (k + 1) * n + j, k * n + i, (k + 1) * n + j, (k + 1) * n + i };
					for (int32 o : Q) Out.Index.Add(First + o);
				}
		}
		void loft(const TArray<FRing>& rings, int32 n, const FVector3f& col, const FVector* ctr = nullptr)
		{
			loft(rings, n, col, ctr, [](int32, int32) { return true; }, false);
		}
		void limb(const FVector& A, const FVector& B, double ra, double rb, int32 n, const FVector3f& col, bool caps, const FVector& ref)   // a tapered tube from A to B, rounded at the ends
		{
			const FVector d = unit(B - A);
			FVector u = cross(d, ref);
			if (len(u) < .05) u = cross(d, FVector(ref.Y, ref.Z, ref.X));
			u = unit(u);
			const FVector v = cross(d, u);
			auto ring = [&](const FVector& c, double r) { return FRing{ c, u * r, v * r }; };
			TArray<FRing> R;
			if (caps) R = { ring(A + d * (-ra * .92), ra * .3), ring(A + d * (-ra * .55), ra * .82), ring(A, ra), ring(B, rb), ring(B + d * (rb * .55), rb * .82), ring(B + d * (rb * .92), rb * .3) };
			else R = { ring(A, ra), ring(B, rb) };
			loft(R, n, col);
		}
		void tube(const TArray<FVector>& pts, const TArray<double>& rs, int32 n, const FVector3f& col, const FVector& ref, bool capA, bool capB)   // one soft form through several points (C1)
		{
			TArray<FRing> rings;
			const int32 m = pts.Num();
			for (int32 i = 0; i < m; ++i)
			{
				const bool bIn = i > 0, bOut = i < m - 1;
				const FVector din = bIn ? unit(pts[i] - pts[i - 1]) : FVector::ZeroVector, dout = bOut ? unit(pts[i + 1] - pts[i]) : FVector::ZeroVector;
				const FVector d = unit(bIn && bOut ? din + dout : bIn ? din : dout);
				FVector u = cross(d, ref);
				if (len(u) < .05) u = cross(d, FVector(ref.Y, ref.Z, ref.X));
				u = unit(u);
				const FVector v = cross(d, u);
				const double r = rs[i] * (bIn && bOut ? FMath::Min(1.35, 1 / FMath::Max(.5, dot(d, dout))) : 1);   // a bend keeps its thickness
				if (i == 0 && capA) { rings.Add({ pts[0] + d * (-r * .92), u * (r * .3), v * (r * .3) }); rings.Add({ pts[0] + d * (-r * .55), u * (r * .82), v * (r * .82) }); }
				rings.Add({ pts[i], u * r, v * r });
				if (i == m - 1 && capB) { rings.Add({ pts[i] + d * (r * .55), u * (r * .82), v * (r * .82) }); rings.Add({ pts[i] + d * (r * .92), u * (r * .3), v * (r * .3) }); }
			}
			loft(rings, n, col);
		}
		static double jaw(double zu) { return zu < 0 ? 1 - .3 * FMath::Pow(-zu, 1.4) : 1; }   // the head narrows to the chin
		// an ellipsoid with half-axes ax, ay, az; taper: a head with a jaw; hood: hangs on lower at the back; keep: which part of the unit sphere to draw
		void ball(const FVector& c, const FVector& ax, const FVector& ay, const FVector& az, int32 n, int32 m, const FVector3f& col,
			bool taper = false, bool hood = false, TFunction<bool(double, double, double)> keep = nullptr)
		{
			TArray<FRing> rings;
			for (int32 k = 0; k <= m; ++k)
			{
				const double f = -UE_DOUBLE_PI / 2 + UE_DOUBLE_PI * k / m, sf = FMath::Sin(f);
				const double cf = FMath::Max(.02, FMath::Cos(f)) * (taper ? jaw(sf) : 1);
				const bool hd = hood && sf < 0;
				rings.Add({ c + az * (hd ? sf * 1.575 : sf), ax * cf, hd ? ay * cf + az * (sf * .325) : ay * cf });
			}
			if (keep)
				loft(rings, n, col, &c, [&](int32 k, int32 i)
				{
					const double f = -UE_DOUBLE_PI / 2 + UE_DOUBLE_PI * (k + .5) / m, a = TAU * (i + .5) / n, cf = FMath::Cos(f);
					return keep(cf * FMath::Cos(a), cf * FMath::Sin(a), FMath::Sin(f));
				}, true);
			else loft(rings, n, col, &c);
		}
	};

	FVector3f scc(const FVector3f& c, double k) { return c * (float)k; }
}

void ResetBrixWalkHeights() { FScopeLock Lock(&BaseLock()); BaseCache().Reset(); }

void DrawBrixPerson(const FBrixPeopleRules& Rules, const FBrixLook& L, const FVector& At, double yaw, const FBrixDrawOptions& o, FBrixBodyMesh& Out, TArray<FBrixFarBox>* OutBoxes)
{
	const double x = At.X, y = At.Y, z = At.Z;
	const int32 pose = o.Pose;
	if (o.Lod >= 3)   // far away: a figure of a few blocks in the person's own colours, legs striding when walking
	{
		if (!OutBoxes) return;
		const double S = L.S, W = L.W;
		const FVector3f legC = L.Legs == TEXT("skirt") ? L.Skin : L.Bottom;
		const double hip = .88 * S, top = hip + .5 * S, fx = FMath::Sin(yaw), fz = -FMath::Cos(yaw);
		auto box = [&](double x0, double y0, double z0, double x1, double y1, double z1, const FVector3f& c)
		{
			OutBoxes->Add({ FVector3f((float)x0, (float)y0, (float)z0), FVector3f((float)x1, (float)y1, (float)z1), c });
		};
		for (const double e : { -1.0, 1.0 })
		{
			const double sw = pose == 1 ? FMath::Sin(o.Phase * 6.283) * .16 * e * S : 0, lx = x + fx * sw - fz * e * .07 * S, lz = z + fz * sw + fx * e * .07 * S;
			box(lx - .065 * S, y, lz - .065 * S, lx + .065 * S, y + hip, lz + .065 * S, legC);
		}
		const double w = .19 * W * S;
		box(x - w, y + hip, z - w, x + w, y + top, z + w, L.Top);
		box(x - .1 * S, y + top + .04 * S, z - .1 * S, x + .1 * S, y + top + .3 * S, z + .1 * S, L.Skin);
		box(x - .11 * S, y + top + .22 * S, z - .11 * S, x + .11 * S, y + top + .33 * S, z + .11 * S, L.Cap ? *L.Cap : L.HairC);
		return;
	}
	const FBrixSignature& sig = Rules.Sig(L.SigName);
	const FGeo g = geo(L.S);
	const double S = L.S, Sh = S * 1.22, W = L.W, T = 1.12 * S, t = o.Seconds;
	const int32 lod = o.Lod, sd = L.Seed;
	const double ff = o.FaceFade;
	const bool midFace = lod == 1 || (lod == 2 && ff > .02);
	double hm[3];
	wander(t, sd * .37, hm);
	const FPose P = pose == 1 ? walk(sig, g, W, o.Phase, hm) : stand(sig, g, W, t, hm, sd);
	// where the character's axes point in the world (standing: x left, y back, z up)
	const double c = FMath::Cos(yaw), s = FMath::Sin(yaw);
	const FVector ex(-c, 0, -s), ey(-s, 0, c), ez(0, 1, 0), O(x, y, z);
	auto Wp = [&](const FVector& p) { return O + ex * p.X + ey * p.Y + ez * p.Z; };
	auto Wd = [&](const FVector& p) { return ex * p.X + ey * p.Y + ez * p.Z; };
	const int32 n = lod == 0 ? 16 : lod == 1 ? 8 : 5, nl = lod == 0 ? 12 : lod == 1 ? 6 : 4;
	const FVector fwd = Wd(FVector(0, -1, 0));
	const bool bLong = L.Garment != TEXT("tee");
	FShapes Sh_{ Out };
	FShapes& ms = Sh_;
	const FString& kind = L.Legs;
	const bool bare = kind != TEXT("trousers");
	const bool lt2 = lod < 2;

	// legs: one form from hip to ankle, bending at the knee; bare legs are thinner, shorts cover the thigh; shoes are rounded ovals
	for (int k = 0; k < 2; ++k)
	{
		const FVector hip = Wp(P.hip[k]), kn = Wp(P.kn[k]), an = Wp(P.an[k]);
		if (bare) ms.tube({ hip, kn, an }, { .08 * T, .0625 * T, .045 * T }, nl, L.Skin, fwd, lt2, false);
		else ms.tube({ hip, kn, an }, { .088 * T, .073 * T, .058 * T }, nl, L.Bottom, fwd, lt2, false);
		if (kind == TEXT("shorts")) ms.limb(hip, hip + (kn - hip) * .82, .092 * T, .086 * T, nl, L.Bottom, lt2, fwd);
		const FVector he = Wp(P.heel[k]), to = Wp(P.toe[k]), f = unit(to - he), up0 = Wd(FVector(0, 0, 1)), side = unit(cross(f, up0)), up = cross(side, f);
		ms.ball(he + f * (.105 * T) + up * (.004 * S), side * (.05 * T), f * (.12 * T), up * (.045 * T), lod == 0 ? 10 : 6, lod == 0 ? 6 : 4, L.Shoes);
	}
	const FVector tx = Wd(mv(P.Rt, FVector(1, 0, 0))), ty = Wd(mv(P.Rt, FVector(0, 1, 0))), tz = Wd(mv(P.Rt, FVector(0, 0, 1)));
	const FVector px = Wd(mv(P.Rp, FVector(1, 0, 0))), py = Wd(mv(P.Rp, FVector(0, 1, 0))), pz = Wd(mv(P.Rp, FVector(0, 0, 1)));
	const FVector pel = Wp(P.pel), sp = Wp(P.spine);
	const double swing = FMath::Abs(P.an[0].Y - P.an[1].Y);
	const double chest = .172 * W, flat = .62 * (1 + .25 * .12);
	// capsule: the script's limb() with flat and cap, built on the hips below the waist and on the chest above; lip: a hem that turns under
	auto capsule = [&](double z0, double r0, double z1, double r1, double flt, double cap, const FVector3f& col, double y0, double y1, double xo, double sway, bool lip)
	{
		TArray<FRing> rings;
		auto at = [&](double zz, double r, double k)
		{
			const double f = (zz - z0) / (z1 - z0), yy = y0 + (y1 - y0) * FMath::Min(1.0, FMath::Max(0.0, f)), d = r * flt * k + (sway ? sway * FMath::Max(0.0, 1 - f * 2.2) : 0);
			if (zz < .1) rings.Add({ pel + (pz * (zz * S) + py * (yy * S) + px * (xo * S)), px * (r * k * S), py * (d * S) });
			else rings.Add({ sp + (tz * ((zz - .098) * S) + ty * (yy * S) + tx * (xo * S)), tx * (r * k * S), ty * (d * S) });
		};
		if (lip) at(z0 + .004, r0, .84);
		else { at(z0 - r0 * cap * .92, r0, .3); at(z0 - r0 * cap * .55, r0, .82); }
		at(z0, r0, 1);
		for (const double f : { .25, .5, .75 }) at(z0 + (z1 - z0) * f, r0 + (r1 - r0) * f, 1);
		at(z1, r1, 1); at(z1 + r1 * cap * .55, r1, .82); at(z1 + r1 * cap * .92, r1, .3);
		ms.loft(rings, n, col);
	};
	// hips, in the colour of what is worn below
	if (kind != TEXT("skirt")) ms.ball(pel + py * (-.01 * T), px * (.16 * W * S), py * (.098 * T), pz * (.1156 * S), n, lod == 0 ? 6 : 4, L.Bottom);
	// the garment: a tee or long top to the waist, a tunic to the thigh, a skirt under either, a coat over it all
	const bool coat = L.Garment == TEXT("coat"), tunic = L.Garment == TEXT("tunic");
	auto topR = [&](double zz) { return tunic ? (.19 * W + (chest - .19 * W) * (zz + .1235) / .545) : (.142 * W + (chest - .142 * W) * (zz - .088) / .3334); };
	const double SK[4] = { coat ? -.5 : -.44, coat ? .21 * W : .225 * W, coat ? .4214 : tunic ? 0 : .2, coat ? chest - .004 : tunic ? topR(0) - .012 : topR(.2) - .012 };
	if (kind == TEXT("skirt")) capsule(SK[0], SK[1], SK[2], SK[3], flat, .28, L.Bottom, 0, 0, 0, swing * .3, true);
	const FVector3f outer = L.Top;
	if (coat)
	{
		capsule(kind == TEXT("skirt") ? -.29 : -.33, .218 * W, .431, chest + .014, flat * 1.06, .25, L.Top, .006, .006, 0, swing * .3, true);
		ms.ball(sp + tz * (.387 * S), tx * (.085 * T), ty * (.08 * T), tz * (.035 * S), n, 4, L.Top);   // the collar
	}
	else if (tunic) capsule(-.1235, .19 * W, .4214, chest, flat, .28, L.Top, 0, 0, 0, swing * .18, true);
	else capsule(.088, .142 * W, .4214, chest, flat, .28, L.Top, 0, 0, 0, 0, true);
	// the yoke: a rounded bar across the shoulders that the arms hang from
	const FVector sh0 = Wp(P.sh[0]), sh1 = Wp(P.sh[1]);
	ms.limb(sh1, sh0, .052 * T, .052 * T, nl, outer, lt2, fwd);
	// arms: one form from shoulder to wrist; a tee has a sleeve to above the elbow; hands are long ovals
	for (int k = 0; k < 2; ++k)
	{
		const FVector a = k ? sh1 : sh0, b = Wp(P.el[k]), w = Wp(P.wr[k]), dir = unit(w - b);
		ms.tube({ a, b, w }, { .052 * T, .0435 * T, .035 * T }, nl, bLong ? outer : L.Skin, fwd, lt2, lt2);
		if (!bLong) ms.limb(a, a + (b - a) * .875, .057 * T, .052 * T, nl, L.Top, lt2, fwd);
		if (lt2) ms.ball(w + dir * (.058 * S), tx * (.03 * T), ty * (.042 * T), dir * (.055 * T), 8, 5, L.Skin);
	}
	// neck and head
	const FVector hx = Wd(mv(P.Rh, FVector(1, 0, 0))) * L.FaceW, hy = Wd(mv(P.Rh, FVector(0, 1, 0))), hz = Wd(mv(P.Rh, FVector(0, 0, 1)));
	const FVector nk = Wp(P.neck), hj = Wp(P.headJ), hc = hj + hz * (.156 * Sh);   // the head's centre, where character_face_05.py puts it
	// things on the front of the face follow the jaw as it narrows towards the chin
	auto H = [&](double a, double b, double cc)
	{
		if (b < -.05) { cc *= 1.08; if (cc < 0) b *= 1 - .27 * FMath::Pow(FMath::Min(1.0, -cc / .14), 1.4); }
		return hc + (hx * (a * Sh) + hy * (b * Sh) + hz * (cc * Sh));
	};
	auto blob = [&](double a, double b, double cc, double rx, double ry, double rz, const FVector3f& col, int32 nn, int32 mm2, bool taper = false, bool hood = false, TFunction<bool(double, double, double)> keep = nullptr)
	{
		ms.ball(H(a, b, cc), hx * (rx * Sh), hy * (ry * Sh), hz * (rz * Sh), nn, mm2, col, taper, hood, MoveTemp(keep));
	};
	ms.limb(nk, hj + hz * (.03 * S), .046 * T, .044 * T, nl, L.Skin, false, fwd);
	const double RX = .104, RY = .11, RZ = .14;   // the head of character_face_05.py, an egg with a jaw and ears
	blob(0, 0, 0, RX, RY, RZ, L.Skin, lod == 0 ? 32 : midFace ? 10 : 6, lod == 0 ? 26 : midFace ? 7 : 4, true);
	if (lod == 0) for (const double e : { -1.0, 1.0 }) blob(e * RX * .97, .012, -.012, .012, .022, .032, L.Skin, 6, 4);
	// hair and hats are shells over the head with the face cut away, so the fringe is a clean line
	const int32 hn = lod == 0 ? 24 : midFace ? 10 : 6, hm2 = lod == 0 ? 16 : midFace ? 6 : 4;
	const FVector3f hairC = L.HairC;
	const FString& st = L.Hair;
	auto shell = [&](double b, double cc, double rx, double ry, double rz, const FVector3f& col, TFunction<bool(double, double, double)> keep, bool hood)
	{
		blob(0, b, cc, rx, ry, rz, col, hn, hm2, false, hood, MoveTemp(keep));
	};
	if (st == TEXT("afro")) shell(.012, .035, RX + .055, RY + .055, RZ + .06, hairC, [](double, double y, double zz) { return !(zz < -.5 || (y < -.35 && zz < .22)); }, false);
	else if (st != TEXT("bald"))
	{
		TFunction<bool(double, double, double)> keep;
		if (st == TEXT("side")) keep = [](double xx, double y, double zz) { return !(zz < -.35 || (y < -.25 && zz < .3 + .28 * xx) || (y < .25 && zz < .05)); };
		else if (st == TEXT("bob") || st == TEXT("long")) keep = [](double, double y, double zz) { return !(zz < -.8 || (y < -.3 && zz < .38)); };
		else keep = [](double, double y, double zz) { return !(zz < -.35 || (y < -.25 && zz < .45) || (y < .25 && zz < .05)); };
		shell(.012, .014, RX + .02, RY + .022, RZ + .02, hairC, MoveTemp(keep), st == TEXT("long"));
		if (st == TEXT("short") || st == TEXT("side")) blob(st == TEXT("side") ? .022 : 0, -.018, .108, .086, .104, .052, hairC, hn, 5);
		if (st == TEXT("long") && lt2) for (const double e : { -1.0, 1.0 }) ms.limb(H(e * .106, 0, 0), H(e * .1, -.018, -.19), .021 * Sh, .013 * Sh, 10, hairC, true, fwd);
		if (st == TEXT("bun")) blob(0, .085, .135, .052, .052, .046, hairC, 8, 5);
	}
	if (L.Cap)
	{
		shell(.006, .016, RX + .03, RY + .032, RZ + .028, *L.Cap, [](double, double, double zz) { return zz >= .12; }, false);
		blob(0, -.135, .062, .09, .075, .009, *L.Cap, 8, 3);
	}
	// the face
	auto jaw = [](double zu) { return FShapes::jaw(zu); };
	auto FS = [&](double xx, double zz, double out)   // a point on the face
	{
		const double zu = zz / RZ, sj = jaw(zu);
		return FVector(xx, -RY * sj * FMath::Sqrt(FMath::Max(1 - FMath::Square(xx / (RX * sj)) - zu * zu, 0.0)) - out, zz);
	};
	auto HR = [&](const FVector& p) { return hc + (hx * (p.X * Sh) + hy * (p.Y * Sh) + hz * (p.Z * Sh)); };
	const FVector3f Dark(.03f, .02f, .02f), White(.78f, .76f, .72f), Lip(.34f, .13f, .11f);
	const FVector3f Cheek(FMath::Min(L.Skin.X * 1.1f, .9f), L.Skin.Y * .66f, L.Skin.Z * .62f);   // rosy cheeks (ADR-0010): a redder version of the person's own skin
	auto cheeks = [&](int32 cn, int32 cm, double f)
	{
		const FVector3f C = f >= 1 ? Cheek : L.Skin + (Cheek - L.Skin) * (float)f;
		for (const double e : { -1.0, 1.0 }) blob(e * .056, -.0835, -.027, .0175, .0062, .0115, C, cn, cm);
	};
	if (lod == 0)   // full eyes, brows, small nose, mouth at rest
	{
		const double period = 2.4 + 3.4 * fmod1(sd * .618), bp = FMath::Fmod(t + sd, period) / .17, lid = bp < 1 ? FMath::Sin(UE_DOUBLE_PI * bp) : 0;
		const double gx = 0, gz = 0;
		const double es = L.EyeSize * 1.15, eo = L.EyeOpen, ns = L.Nose;   // the reference's eyes are 1.15 times natural size
		const double eX = .043, eZ = .008 * RZ / .13, ew = .0245 * es, eh = .021 * es * eo, ir = .0155 * es, irz = FMath::Min(ir, eh * .97), yawE = FMath::Asin(FMath::Min(eX / RX, 1.0)) * .55, tilt = .1;
		for (const double e : { -1.0, 1.0 })
		{
			const double th = e * yawE, ph = -e * tilt, cc = FMath::Cos(th), sn = FMath::Sin(th), cp = FMath::Cos(ph), sp2 = FMath::Sin(ph);
			const double R[3][3] = { { cc * cp, sn * cp, -sp2 }, { -sn, cc, 0 }, { cc * sp2, sn * sp2, cp } };   // the eye's own across, out and up, in head axes
			auto ax = [&](int k) { return hx * R[k][0] + hy * R[k][1] + hz * R[k][2]; };
			const FVector AX = ax(0), AY = ax(1), AZ = ax(2);
			auto part = [&](const FVector& ctr, double rx, double ry, double rz, const FVector3f& col, int32 nn, int32 mm2, TFunction<bool(double, double, double)> keep = nullptr)
			{
				ms.ball(ctr, AX * (rx * Sh), AY * (ry * Sh), AZ * (rz * Sh), nn, mm2, col, false, false, MoveTemp(keep));
			};
			const FVector wc = FS(e * eX, eZ, -.005);
			auto onEye = [&](double u, double w, double lift)
			{
				const double uu = cl(u / ew, -.9, .9), ww = cl(w / eh, -.9, .9), d = -(.011 * FMath::Sqrt(FMath::Max(.05, 1 - uu * uu - ww * ww)) + lift);
				return HR(FVector(wc.X + R[0][0] * u + R[1][0] * d + R[2][0] * w, wc.Y + R[0][1] * u + R[1][1] * d + R[2][1] * w, wc.Z + R[0][2] * u + R[1][2] * d + R[2][2] * w));
			};
			if (lid >= .03)   // the upper lid: a skin shape that grows down over the eye, with the lash line on its lower edge
			{
				part(HR(FS(e * eX, eZ + eh * (1 - lid), .005)), ew * 1.03, .0055, FMath::Max(eh * lid * 1.08, .0004), L.Skin, 28, 10);
				part(HR(FS(e * eX, eZ + eh * (1 - 2.08 * lid) - .001, .0075)), ew * 1.04, .005, FMath::Max(eh, .019) * .1, Dark, 24, 6);
				if (lid > .97) continue;
			}
			else part(HR(FS(e * eX, eZ + .0012, -.005)), ew * 1.05, .0092, eh * 1.05, Dark, 28, 16, [](double, double yy, double zz) { return zz > .18 && yy < .3; });   // open: the lash line over the white
			part(HR(wc), ew, .011, eh, White, 28, 14);   // the white
			part(onEye(gx, gz - .001 * es, .0003), ir, .0014, irz, L.Eyes, 28, 6);
			part(onEye(gx, gz - .001 * es, .0013), .0068 * es, .001, FMath::Min(.0068 * es, irz), Dark, 24, 5);
			part(onEye(gx + .005, gz + .005 - .001 * es, .002), .0034, .001, .0034, White, 12, 4);
		}
		const double by = (.008 + .0445 * FMath::Max(1.0, es * .9)) * RZ / .13 + L.Brow;
		for (const double e : { -1.0, 1.0 }) ms.limb(HR(FS(e * (eX - .02), by, .0045)), HR(FS(e * (eX + .025), by + .004, .0045)), .0048 * Sh, .0032 * Sh, 12, scc(hairC, .6), true, hy);
		ms.ball(HR(FS(0, -.021 * RZ / .13, .003)), hx * (.0115 * ns * .72 * Sh), hy * (.013 * ns * .72 * Sh), hz * (.017 * ns * .72 * Sh), 18, 12, L.Skin);
		cheeks(10, 4, 1);
		blob(0, -.1, -.053, .032 * L.Lips, .005, .0062, Lip, 10, 3);
		if (L.bTash) blob(0, -.104, -.037, .024, .006, .007, hairC, 6, 3);
	}
	else if (midFace)   // the middle distance (to 16 m): the same face in few divisions; beyond 16 m faded into the skin
	{
		auto fc = [&](const FVector3f& C) { return ff >= 1 ? C : L.Skin + (C - L.Skin) * (float)ff; };
		const double es = L.EyeSize * 1.15, eX = .043, eZ = .008 * RZ / .13, ew = .0245 * es, eh = .021 * es * L.EyeOpen, ir = .0155 * es;
		for (const double e : { -1.0, 1.0 })
		{
			ms.ball(HR(FS(e * eX, eZ, -.005)), hx * (ew * Sh), hy * (.011 * Sh), hz * (eh * Sh), 10, 6, fc(White));
			ms.ball(HR(FS(e * eX, eZ - .001 * es, .0045)), hx * (ir * Sh), hy * (.002 * Sh), hz * (FMath::Min(ir, eh * .97) * Sh), 10, 4, fc(L.Eyes));
			ms.ball(HR(FS(e * eX, eZ - .001 * es, .0058)), hx * (.0068 * es * Sh), hy * (.0015 * Sh), hz * (FMath::Min(.0068 * es, ir) * Sh), 8, 3, fc(Dark));
			ms.ball(HR(FS(e * eX, eZ + eh * .95, -.002)), hx * (ew * 1.05 * Sh), hy * (.004 * Sh), hz * (.003 * Sh), 10, 3, fc(Dark));
		}
		const double by = (.008 + .0445 * FMath::Max(1.0, es * .9)) * RZ / .13 + L.Brow;
		for (const double e : { -1.0, 1.0 }) ms.limb(HR(FS(e * (eX - .02), by, .0045)), HR(FS(e * (eX + .025), by + .004, .0045)), .0048 * Sh, .0032 * Sh, 6, fc(scc(hairC, .6)), true, hy);
		ms.ball(HR(FS(0, -.021 * RZ / .13, .003)), hx * (.0115 * L.Nose * .72 * Sh), hy * (.013 * L.Nose * .72 * Sh), hz * (.017 * L.Nose * .72 * Sh), 8, 5, L.Skin);
		cheeks(6, 3, ff);
		blob(0, -.1, -.053, .032 * L.Lips, .005, .0062, fc(Lip), 8, 3);
		if (L.bTash) blob(0, -.104, -.037, .024, .006, .007, fc(hairC), 6, 3);
	}
	// What the role wears on top (C5) lies on the outside of the clothes under it. wear(z, x) is that outside at height z and x
	// across: [front, back] depth in S.
	struct FLayer { double z0, r0, z1, r1, fl, cap, yo, sw; };
	TArray<FLayer> layers;
	if (kind == TEXT("skirt")) layers.Add({ SK[0], SK[1], SK[2], SK[3], flat, .28, 0, swing * .3 });
	if (coat) layers.Add({ kind == TEXT("skirt") ? -.29 : -.33, .218 * W, .431, chest + .014, flat * 1.06, .25, .006, swing * .3 });
	else if (tunic) layers.Add({ -.1235, .19 * W, .4214, chest, flat, .28, 0, swing * .18 });
	else layers.Add({ .088, .142 * W, .4214, chest, flat, .28, 0, 0 });
	auto wear = [&](double zz, double xx)
	{
		double f = 0, b = 0;
		if (kind != TEXT("skirt")) { const double e = 1 - FMath::Square(zz / .1156) - FMath::Square(xx / (.16 * W)); if (e > 0) { const double r = .1098 * FMath::Sqrt(e); f = r + .0112; b = r - .0112; } }   // the hips
		for (const FLayer& Y : layers)
		{
			if (zz < Y.z0) continue;
			const double q = FMath::Min(1.0, (zz - Y.z0) / (Y.z1 - Y.z0)), cc = zz > Y.z1 ? (zz - Y.z1) / (Y.r1 * Y.cap * .92) : 0;
			if (cc >= 1) continue;
			const double k = FMath::Sqrt(1 - cc * cc), rx = (Y.r0 + (Y.r1 - Y.r0) * q) * k, e2 = 1 - FMath::Square(xx / rx);
			if (e2 <= 0) continue;
			const double d = (rx * Y.fl + (Y.sw ? Y.sw * FMath::Max(0.0, 1 - q * 2.2) : 0)) * FMath::Sqrt(e2);
			f = FMath::Max(f, d - Y.yo); b = FMath::Max(b, d + Y.yo);
		}
		return FVector2D(f, b);
	};
	auto Tp = [&](double zz, double xx, double yy) { return zz < .1 ? pel + (pz * (zz * S) + py * (yy * S) + px * (xx * S)) : sp + (tz * ((zz - .098) * S) + ty * (yy * S) + tx * (xx * S)); };
	auto Np = [&](double xx, double yy, double zz) { return nk + (tx * (xx * S) + ty * (yy * S) + tz * (zz * S)); };   // round the neck, over the yoke
	// the apron: a flat bib from the chest to the thigh, hung so it clears the clothes and the legs as they swing forward
	bool bApY = false;
	double ay0 = 0, ay1 = 0;
	const double AZ0 = -.2565, AZ1 = .2842;
	auto apD = [&](double zz) { return (.118 + (.1 - .118) * (zz - AZ0) / (AZ1 - AZ0)) * W * .26; };
	auto apY = [&](double zz) { return ay0 + (ay1 - ay0) * cl((zz - AZ0) / (AZ1 - AZ0), 0, 1); };
	if (L.Apron && lt2)
	{
		const double legR[3] = { bare ? .08 : .088, bare ? .0625 : .073, bare ? .045 : .058 };
		auto legFront = [&](double zz)   // the front of the legs at height z, as the bib's y (negative is forward)
		{
			double m = 1;
			const double zh = P.pel.Z + zz * S;
			for (int k = 0; k < 2; ++k)
			{
				const FVector& h = P.hip[k]; const FVector& kn = P.kn[k]; const FVector& an = P.an[k];
				const bool bUp = zh >= kn.Z;
				const FVector& a = bUp ? h : kn; const FVector& b = bUp ? kn : an;
				const double den = (a.Z - b.Z) != 0 ? (a.Z - b.Z) : 1e-6;
				const double q = cl((a.Z - zh) / den, 0, 1), r = (bUp ? legR[0] + (legR[1] - legR[0]) * q : legR[1] + (legR[2] - legR[1]) * q) * T;
				if (zh <= h.Z + r) m = FMath::Min(m, (a.Y + (b.Y - a.Y) * q - r - P.pel.Y) / S);
			}
			return m;
		};
		auto need = [&](double zz) { return FMath::Min(-(wear(zz, 0).X + apD(zz) + .006), legFront(zz) - apD(zz) - .006); };
		double y1 = 1;
		for (const double zz : { .2, .25, AZ1 }) y1 = FMath::Min(y1, need(zz));
		double y0 = need(AZ0);
		for (double zz = AZ0 + .05; zz < AZ1 - .04; zz += .05) { const double q = (zz - AZ0) / (AZ1 - AZ0); y0 = FMath::Min(y0, (need(zz) - y1 * q) / (1 - q)); }
		ay0 = y0; ay1 = y1; bApY = true;
		capsule(AZ0, .118 * W, AZ1, .1 * W, .26, .2, *L.Apron, y0, y1, 0, 0, false);
		for (const double e : { -1.0, 1.0 })   // the straps: up from the bib's corners, over the chest, round the back of the neck
		{
			const double zc = .38, xs = e * .075 * W;
			ms.tube({ Tp(.28, e * .07 * W, y1 - apD(AZ1) * .6), Tp(zc, xs, -(wear(zc, xs).X + .014)), Np(e * .068, -.012, .062), Np(e * .045, .064, .034), Np(0, .07, .026) },
				{ .008 * S, .008 * S, .008 * S, .008 * S, .008 * S }, 4, *L.Apron, fwd, false, false);
		}
	}
	// the scarf tail lies on the chest: a thin flat strip from under the wrap down the front, in short pieces that follow the chest
	const double SZ0 = bApY ? AZ1 + .035 : .2893, SZ1 = .47;
	auto yokeF = [](double zz) { return FMath::Sqrt(FMath::Max(0.0, FMath::Square(.052 * 1.12) - FMath::Square(zz - .436))); };
	auto sNeed = [&](double zz) { return -(FMath::Max(wear(zz, .042).X, yokeF(zz)) + .012); };
	struct FSeg { double a, b, ya, yb; };
	TArray<FSeg> scarfSegs;
	if (L.Scarf && lt2)
	{
		const int32 sn = 4;
		double zs[sn + 1];
		for (int32 k = 0; k <= sn; ++k) zs[k] = SZ0 + (SZ1 - SZ0) * k / sn;
		for (int32 k = 0; k < sn; ++k)
		{
			const double a = zs[k], b = zs[k + 1], ya = sNeed(a), yb = sNeed(b), e = FMath::Max(0.0, (ya + yb) / 2 - sNeed((a + b) / 2));
			scarfSegs.Add({ a, b, ya - e, yb - e });
		}
	}
	auto scarfY = [&](double zz)
	{
		const FSeg* G = &scarfSegs.Last();
		for (const FSeg& Q : scarfSegs) if (zz <= Q.b) { G = &Q; break; }
		return G->ya + (G->yb - G->ya) * cl((zz - G->a) / (G->b - G->a), 0, 1);
	};
	auto over = [&](double zz, double xx)   // the outside at the front, apron and scarf tail included
	{
		double f = wear(zz, xx).X;
		if (bApY && zz > AZ0 - .02 && zz < AZ1 + .02 && FMath::Abs(xx) < .118 * W) f = FMath::Max(f, -apY(zz) + apD(zz));
		if (scarfSegs.Num() && zz > SZ0 - .01 && zz < SZ1 && FMath::Abs(xx - .042) < .045) f = FMath::Max(f, -scarfY(zz) + .012);
		return f;
	};
	if (L.Scarf)   // a wrap round the neck and a flat tail down the front of the chest
	{
		const double big = coat ? 1.15 : 1;   // over a coat's collar
		ms.ball(nk + tz * (.049 * S), tx * (.088 * T * big), ty * (.084 * T * big), tz * (.04 * S), hn, 4, *L.Scarf);
		if (scarfSegs.Num())
		{
			TArray<FVector2D> pts;   // (z, y)
			pts.Add(FVector2D(scarfSegs[0].a, scarfSegs[0].ya));
			for (int32 k = 0; k < scarfSegs.Num(); ++k) pts.Add(FVector2D(scarfSegs[k].b, FMath::Min(scarfSegs[k].yb, k + 1 < scarfSegs.Num() ? scarfSegs[k + 1].ya : scarfSegs[k].yb)));
			const double r = .029, fl2 = .25;
			TArray<FRing> rings;
			auto ring = [&](double zz, double yy, double k) { rings.Add({ sp + (tz * ((zz - .098) * S) + ty * (yy * S) + tx * (.042 * S)), tx * (r * k * S), ty * (r * fl2 * k * S) }); };
			ring(pts[0].X - r * .37, pts[0].Y, .3); ring(pts[0].X - r * .2, pts[0].Y, .82);
			for (const FVector2D& Q : pts) ring(Q.X, Q.Y, 1);
			const FVector2D E = pts.Last();
			ring(E.X + r * .2, E.Y, .82); ring(E.X + r * .37, E.Y, .3);
			ms.loft(rings, n, *L.Scarf);
		}
	}
	if (L.bBag && lt2)   // the bag at the left hip, its strap over the right shoulder, lying on whatever is worn
	{
		const FVector3f Brown = Rules.Palette.Contains(TEXT("brown")) ? Rules.Palette[TEXT("brown")] : FVector3f(.24f, .16f, .11f);
		const FVector bg = pel + (px * (.225 * S) + py * (-.1 * T));
		const double sr = .012;
		ms.ball(bg, px * (.05 * S), py * (.1 * S), pz * (.09 * S), 8, 5, Brown);
		TArray<FVector> pts = { bg + (pz * (.085 * S) + py * (-.02 * S)) };   // from the top of the bag, up across the front to the right shoulder
		for (const double f : { .12, .3, .48, .66, .84 }) { const double zz = .1 + .33 * f, xx = (.16 - .26 * f) * W; pts.Add(Tp(zz, xx, -(over(zz, xx) + sr + .004))); }
		pts.Add(Tp(.515, -.1, 0));   // over the shoulder, above the yoke
		for (const double f : { .16, .45, .75 }) { const double zz = .43 - .31 * f, xx = (-.1 + .26 * f) * W; pts.Add(Tp(zz, xx, wear(zz, xx).Y + sr + .004)); }   // and down the back
		pts.Add(bg + (pz * (.075 * S) + py * (.06 * S)));   // to the back of the bag
		TArray<double> rs;
		rs.Init(sr * S, pts.Num());
		ms.tube(pts, rs, 4, Brown, fwd, false, false);
	}
}
