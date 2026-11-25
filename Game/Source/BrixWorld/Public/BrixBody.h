#pragma once

#include "CoreMinimal.h"

// The slice's people (Prototype/play/people.js) in C++: who someone is from their role (look()), how they walk and stand
// (the movement signatures of docs/art/CHARACTER_MOTION.md, a port of Tools/blender/motion_lib.py by way of people.js),
// and their option B body, clothes, hair and face as one mesh built from lofted rings (draw()). The same numbers as the
// slice, from Game/Data/people.json (written from people.js by Tools/unreal/people_values.py); checked against people.js
// itself by the test ART004_people_are_drawn_as_the_slice_draws_them. These are the code-made people that stand in until
// the production models (Q-15) and the walk's final form (Q-17) are decided.
//
// Coordinates are the slice's: metres, x east, y up, z south. A body faces (sin yaw, -cos yaw) in x and z.

// A movement signature (CHARACTER_MOTION §3): every number people.js's BASE and SIG hold.
struct BRIXWORLD_API FBrixSignature
{
	FString Name;
	double Cycle = 24, Stride = .84, Duty = .62, Fore = .42, Bounce = .024, Sway = .018, HipYaw = 6, HipRoll = 3.5, Counter = 1,
		Lean = 3, Width = .75, ToeOut = 7, Lift = .085, Strike = 16, ToeOff = 42, ArmSwing = 18, ArmBias = 2, ArmOut = 2.5,
		Elbow = 16, ElbowSwing = 14, ArmL = 1, ArmR = 1, HeadPitch = 0, HeadBob = 1.2, HeadFollow = .25, HeadLag = .06;
};

// What a role wears and how its people are made (people.js ROLE): lists of palette names to pick from.
struct BRIXWORLD_API FBrixRoleWear
{
	FString Garment = TEXT("coat");
	TArray<FString> Top, Bottom, Apron, Cap, Scarf, Hair, Shoes;
	double Skirt = 0, Shorts = 0, Bag = 0, ScarfP = 0, Tash = 0;
	bool bGrey = false;
	FVector2D Height = FVector2D(1.6, 1.8), Build = FVector2D(.9, 1.1);
};

// One person's look (people.js look()): sizes, colours, clothes, the walk.
struct BRIXWORLD_API FBrixLook
{
	double S = 1, W = 1;                        // height / 1.72, build
	FVector3f Skin, HairC, Eyes, Top, Bottom, Shoes;
	TOptional<FVector3f> Apron, Cap, Scarf;
	FString Hair = TEXT("short"), Garment = TEXT("coat"), Legs = TEXT("trousers");
	bool bBag = false, bTash = false;
	FString SigName = TEXT("Easy");
	int32 Seed = 0;
	double FaceW = 1, EyeSize = 1, EyeOpen = 1, Brow = 0, Nose = 1, Lips = 1;   // the face controls at rest (people.js FM0)
};

struct BRIXWORLD_API FBrixPeopleRules
{
	TMap<FString, FBrixSignature> Signatures;
	TMap<FString, FVector3f> Palette;
	TArray<FVector3f> Skins, Hairs, Eyes;
	TMap<FString, FBrixRoleWear> Roles;
	FBrixLook Player;                           // the player's look (people.js player()) until the designed character (CHR-001)
	const FBrixSignature& Sig(const FString& Name) const;
};

// Reads Game/Data/people.json. On failure returns false and says why.
BRIXWORLD_API bool LoadBrixPeopleRules(const FString& Path, FBrixPeopleRules& Out, FString& OutError);

// Resident Id's look: their role's clothes (a job's own where it has them, CITY-105) and their role's walk.
BRIXWORLD_API FBrixLook BrixLookOf(const FBrixPeopleRules& Rules, int32 Id, const FString& Role, const FString& SigName);

// A body as indexed triangles: each lofted form's ring points once, with a normal and a colour each, and three indices a
// triangle. Expanded (Pos[Index[k]] for each k) it is the triangle list people.js writes, three vertices a triangle.
struct BRIXWORLD_API FBrixBodyMesh
{
	TArray<FVector3f> Pos, Normal;
	TArray<FVector3f> Colour;
	TArray<int32> Index;
	void Reset() { Pos.Reset(); Normal.Reset(); Colour.Reset(); Index.Reset(); }
};

// A far figure's box (people.js far()): corners and colour, not turned.
struct BRIXWORLD_API FBrixFarBox
{
	FVector3f Min, Max, Colour;
};

// The face control set of docs/art/CHARACTER_FACE.md §1 (people.js face()), as a conversation drives it: the mouth's
// shape, the smile or frown, each brow's inner and outer end, the lids, a squint, how wide the eyes open, the gaze, a nod.
struct BRIXWORLD_API FBrixFace
{
	struct FMouth { double Width = .0175, Opening = .0011, Round = 0; int32 Teeth = 0; bool bTongue = false; double Curve = 0; };
	FMouth Mouth;
	double Curve = .075;
	double BrowL[2] = { 0, 0 }, BrowR[2] = { 0, 0 };
	double Lid = 0, Squint = 0, Open = 1;
	double Gaze[2] = { 0, 0 };
	double Nod = 0;
};

struct BRIXWORLD_API FBrixDrawOptions
{
	int32 Pose = 0;          // 0 standing, 1 walking, 2 sitting (the origin is then the seat's top)
	double Phase = 0;        // the walk's cycle, 0..1
	double Seconds = 0;      // time, for breathing, blinking and the head's wander
	int32 Lod = 1;           // 0 full (the nearest few), 1 to 16 m, 2 to 35 m; 3 a far figure of boxes
	double FaceFade = 1;     // at lod 2, how much of the face still shows (0 none)
	TOptional<double> Floor; // sitting in a car: the floor this far below the seat, the feet out along it (people.js mod.floor)
	bool bDriving = false;   // sitting at a wheel: the hands up on it (cars.js seat(): arms [1, 58, 10, 48])
	const FBrixFace* Face = nullptr;   // in a conversation: the face as it speaks (full detail only), and its nod
	// people.js o.sig: walk by this signature instead of the look's own (a run from a car, a hurry in the rain)
	FString Sig;
	// people.js o.mod, the gesture layers of CHARACTER_MOTION §4: per arm (left, right) [weight, raise, out, elbow] in
	// degrees, blended over the pose by weight; the chest's [pitch, yaw] in degrees, added
	TOptional<FVector4d> Arms[2];
	FVector2D Chest = FVector2D::ZeroVector;
	TOptional<FVector3f> Hood;         // people.js o.hood: a coat's hood up, in this colour (CITY-080)
	TOptional<FVector3f> Umbrella;     // people.js o.umb: an umbrella up in the right hand, its canopy in this colour (CITY-080)
};

// people.js draw() for poses 0 to 2: appends the body to Out (lods 0 to 2) or its boxes to OutBoxes (lod 3).
BRIXWORLD_API void DrawBrixPerson(const FBrixPeopleRules& Rules, const FBrixLook& L, const FVector& At, double Yaw,
	const FBrixDrawOptions& O, FBrixBodyMesh& Out, TArray<FBrixFarBox>* OutBoxes = nullptr);

// Metres covered by one whole cycle of a walk (two steps): its phase is metres walked / this.
BRIXWORLD_API double BrixStepLength(const FBrixSignature& Sig, double S);

// people.js keeps each walk's hip height by signature and height to two decimals, so who was drawn first sets it for
// others of nearly the same height; this forgets them (a test drawing in the slice's order starts from here).
BRIXWORLD_API void ResetBrixWalkHeights();
