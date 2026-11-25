#include "BrixEncodeOggCommandlet.h"

#include "BrixSound.h"
#include "Interfaces/IAudioFormat.h"
#include "Interfaces/IAudioFormatModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

int32 UBrixEncodeOggCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString Dir, Files;
	int32 Quality = 40;
	FParse::Value(*Params, TEXT("Dir="), Dir, false);
	FParse::Value(*Params, TEXT("Files="), Files, false);
	FParse::Value(*Params, TEXT("Quality="), Quality);
	IAudioFormatModule* Module = FModuleManager::Get().LoadModulePtr<IAudioFormatModule>(TEXT("AudioFormatOgg"));
	const IAudioFormat* Ogg = Module ? Module->GetAudioFormat() : nullptr;
	if (!Ogg) { UE_LOG(LogTemp, Error, TEXT("BrixEncodeOgg: the engine's Ogg encoder (AudioFormatOgg) is not there")); return 1; }
	TArray<FString> List;
	Files.ParseIntoArray(List, TEXT(","));
	int32 Done = 0;
	for (const FString& Name : List)
	{
		TArray<uint8> File, Pcm, Out;
		int32 Rate = 0, Ch = 0;
		const FString In = FPaths::Combine(Dir, Name + TEXT(".wav"));   // names come without the extension: the command line splits "a.wav" at its dot
		if (!FFileHelper::LoadFileToArray(File, *In) || !ReadBrixWav(File, Pcm, Rate, Ch)) { UE_LOG(LogTemp, Error, TEXT("BrixEncodeOgg: %s is not a 16-bit WAV"), *In); return 1; }
		FSoundQualityInfo Q;
		Q.Quality = Quality;
		Q.NumChannels = (uint32)Ch;
		Q.SampleRate = (uint32)Rate;
		Q.SampleDataSize = (uint32)Pcm.Num();
		Q.Duration = (float)Pcm.Num() / (2.f * Ch * Rate);
		Q.DebugName = Name;
		if (!Ogg->Cook(FName(TEXT("OGG")), Pcm, Q, Out) || !Out.Num()) { UE_LOG(LogTemp, Error, TEXT("BrixEncodeOgg: %s did not encode"), *In); return 1; }
		const FString Path = FPaths::ChangeExtension(In, TEXT("ogg"));
		if (!FFileHelper::SaveArrayToFile(Out, *Path)) { UE_LOG(LogTemp, Error, TEXT("BrixEncodeOgg: cannot write %s"), *Path); return 1; }
		UE_LOG(LogTemp, Display, TEXT("BrixEncodeOgg: %s, %.1f s, %d KB to %d KB"), *Name, Q.Duration, Pcm.Num() / 1024, Out.Num() / 1024);
		++Done;
	}
	UE_LOG(LogTemp, Display, TEXT("BrixEncodeOgg: %d files"), Done);
	return 0;
#else
	return 1;
#endif
}
