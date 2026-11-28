#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BrixLight.generated.h"

// The eye and the night (ART_DIRECTION W5 to W7; WLD-042, ADR-0023, ADR-0055). The game mode makes the sun, the sky's fill
// and the haze and turns them by the clock; this adds what the picture still needs as the sun gets low and once it is
// down, and is made by the engine for every game world, so nothing has to spawn it. The arithmetic is BrixWorld's
// BrixNight (BrixLightNow), which has the test:
//   - the eye: the engine's automatic exposure opens without limit as the light goes, which made the night as bright as
//     the day (a bright orange fog). It may now open only so far past the day's exposure: freely by day (the approved
//     daylight look indoors and out is untouched, ADR-0048), DuskOpen stops with the sun on the horizon, NightOpen at
//     night, a little more in a room;
//   - the haze at dusk, dawn and night: the game mode turns the whole haze the low sun's orange at the day's full
//     strength while the light itself falls away, so dusk and dawn were an orange white-out and the night kept the
//     orange. A low sun's haze is now dimmer and cooler with the sun's colour only in the air towards it, and once the
//     day is gone it is the slice's dark blue distance (sky.json night.horizon and city_glow);
//   - the lamps: they and the lit windows came on by the calendar's daylight alone, half an hour and more after the
//     streets had gone dark. They now come on as the sun gets low (LowSun());
//   - the night's light: with the sun out the sky was black and gave the shade no fill at all. A faint blue light from
//     high up (the slice's own moon direction when its moon is down) lights the air, so the sky is a deep blue and the
//     sky light has something to fill the streets with; it casts shadows, so roofs keep it out of rooms.
// Not here yet: the stars, the Milky Way, the moon's disc, path and phases (WLD-042's sky). (No planet: WLD-065 is cut.)
//
// For looking at the light in many places in one run (a launch costs far more than a shot):
//   -BrixTour=<file> -BrixTourOut=<folder>   each line "name|hh:mm|weather|where[|ev=<EV100>]": set the clock and the
//       weather (empty: the day's own), go there, wait TourSeconds, save <folder>/<name>.png; quit after the last.
//       where is cam:x,z,height,yaw,pitch (look from there), stand:x,z,up,yaw (the player there, the camera behind) or
//       in:<lot id>[,<pitch>] (the player inside that building, as -BrixInside= puts them, looking up or down by
//       pitch); metres and degrees in the data's frame, yaw 0 = east. ev= pins the exposure for that shot, to measure.
//       pt=<seconds> takes that shot with the engine's path tracer instead (the slow, correct light of the same scene,
//       to measure the live light against): after the wait the world is paused, the path tracer gathers PathSamples
//       samples a pixel for that long, then the shot. It needs ray tracing, which the project does not compile by
//       default: run it with Tools/unreal/light_tour.ps1 -PathTrace.
//       Run it with -BrixNoDesigner -BrixNoSave (not -BrixShot=, which quits after one shot).
//       With -BrixLightRoom (ABrixLightRoom: the Blender reference rooms built in Unreal) where may be
//       refroom:<shop|cafe|kitchen|home|hall>, the reference's own camera in that room, and hh:mm may be ref, when the
//       game's sun stands as the references' does. home and hall are the golden room and its hallway (ADR-0073): the log
//       then says where their grey card and ball are in the shot ("card x0,y0,x1,y1 ball x,y,r", shares of the picture).
//   -BrixLightOff                             leave the light as the game mode makes it, to compare
//   -BrixLightNo=<letters>                    leave parts out: e the eye, h the haze, n the night's light, s its sky
UCLASS(Config = Game)
class BRIXTOWN_API UBrixLight : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickableWhenPaused() const override { return true; }   // a pt= stop pauses the world for the path tracer
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UBrixLight, STATGROUP_Tickables); }
	// 0 with the sun up to 1 with it on the horizon or down: by then the streets are in shade, whatever the calendar's
	// daylight says, so the lamps and the lit windows come on by this too (ABrixLamps, ABrixRooms)
	float LowSun() const { return LowSunNow; }
	// whether this sets the haze's colour (not with -BrixLightOff or -BrixLightNo=h): the game mode then leaves it alone,
	// so it is set once a frame
	static bool SetsHaze();

	// the eye. DayEV: the exposure (EV100) the engine settles on in the open street at noon, measured with a tour's ev=;
	// the others are stops it may open past that
	UPROPERTY(Config) float DayEV = 1.3f;
	UPROPERTY(Config) float DayOpen = 2.2f;            // by day out of doors: free it burnt a shaded alley's sunlit end white; the open street never opens this far
	UPROPERTY(Config) float DayOpenIndoors = 2.4f;     // in a room by day: where the reference rooms built in Unreal are as bright as Blender's (ABrixLightRoom)
	UPROPERTY(Config) float DuskOpen = 3.f;            // out of doors with the sun on the horizon, when the sky alone lights the streets
	UPROPERTY(Config) float NightOpen = 2.f;           // at night out of doors
	UPROPERTY(Config) float NightOpenIndoors = 2.5f;   // at night in a room, by its lamps
	UPROPERTY(Config) float IndoorSeconds = 1.5f;      // how long the eye takes going in or out
	// only the stronger of the sun and the night's light casts shadows (off: both, as it was, to measure against)
	UPROPERTY(Config) bool bOneSkyShadow = true;
	// the night comes in as the day's light (ABrixGameMode::Daylight) falls from NightBelow to 0
	UPROPERTY(Config) float NightBelow = 0.3f;
	UPROPERTY(Config) FLinearColor NightHaze = FLinearColor(0.030f, 0.032f, 0.042f);   // the distance at night
	// a low sun's haze: below LowSunBelow of the game mode's measure of the sun's height (1 with the sun well up) the haze
	// turns from the game mode's to HazeColour x LowSunHaze, and the glow of the air towards the sun from the engine's own
	// to the sun's colour x LowSunGlow
	UPROPERTY(Config) float LowSunBelow = 0.6f;
	UPROPERTY(Config) FLinearColor LowSunHaze = FLinearColor(0.14f, 0.15f, 0.24f);
	UPROPERTY(Config) float LowSunGlow = 0.1f;
	UPROPERTY(Config) float NightLux = 0.35f;          // the night's light on what faces it (the sun's is SunLux)
	UPROPERTY(Config) FLinearColor NightColour = FLinearColor(0.48f, 0.62f, 1.f);      // sky.json night.moon.light's colour
	UPROPERTY(Config) FVector NightFrom = FVector(-0.42, -0.52, 0.74);                 // towards it: x east, y south, z up
	UPROPERTY(Config) FLinearColor NightSky = FLinearColor(2.f, 2.3f, 3.f);            // how much the air gives back of it: the sky's blue and its fill
	UPROPERTY(Config) float TourSeconds = 6.f;         // -BrixTour: the wait at each stop before its shot
	UPROPERTY(Config) int32 PathSamples = 512;         // -BrixTour: a pt= stop's samples a pixel

private:
	void Light(class ABrixGameMode& Mode, float DeltaSeconds);
	void Tour(class ABrixGameMode& Mode, float DeltaSeconds);

	UPROPERTY() TObjectPtr<class UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<class USkyAtmosphereComponent> Air;
	UPROPERTY() TObjectPtr<class ADirectionalLight> Sun;          // the game mode's
	UPROPERTY() TObjectPtr<AActor> Holder;                        // carries the two below
	UPROPERTY() TObjectPtr<class UPostProcessComponent> Eye;
	UPROPERTY() TObjectPtr<class UDirectionalLightComponent> NightLight;
	UPROPERTY() TObjectPtr<class ACameraActor> TourCam;
	UPROPERTY() TObjectPtr<class ABrixLightRoom> RefRooms;        // -BrixLightRoom: the Blender reference rooms, to measure against
	float TryLamp = 1.f, TryFill = -1.f;                          // a tour stop's numbers for them
	TOptional<FLinearColor> TryTint, TryFillTint;
	bool bLooked = false, bOff = false;
	FString Without;                                               // -BrixLightNo=: parts left out
	float Indoors = 0.f, LowSunNow = 0.f;
	FLinearColor SkySet = FLinearColor(-1, -1, -1), DayGlow = FLinearColor::Black;
	TOptional<float> PinEV;

	struct FStop { FString Name, Weather, Lot, RefRoom; double Minute = -1; bool bRefTime = false; bool bCam = false; FVector At = FVector::ZeroVector; float Yaw = 0.f, Pitch = 0.f; TOptional<float> EV; float Lamp = 1.f, Fill = -1.f; TOptional<FLinearColor> Tint, FillTint; float PathSeconds = 0.f; };
	TArray<FStop> Stops;
	FString TourOut;
	int32 StopAt = -1;
	float StopWait = 0.f;
	float StopTime = 0.f, StopWorst = 0.f;   // the frames of the last two seconds of a stop's wait: their time, the longest
	int32 StopFrames = 0;
	bool bShot = false;
	bool bPathing = false;          // a pt= stop: paused, the path tracer gathering
	double PathUntil = 0.0, LastReal = 0.0;
};
