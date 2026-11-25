#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BrixEncodeOggCommandlet.generated.h"

// Makes WAV recordings Ogg Vorbis with the engine's own encoder (AudioFormatOgg), for Tools/unreal/sound_values.py: the
// music is the only long recording, and as WAV it would be ten times the size. Editor builds only.
//   UnrealEditor-Cmd.exe BrixTown.uproject -run=BrixEncodeOgg -Dir=<folder> -Files=a,b [-Quality=40]
// reads a.wav and b.wav and writes a.ogg and b.ogg beside them. The game reads them with the engine's Vorbis decoder, as it reads the talk's voices.
UCLASS()
class UBrixEncodeOggCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
