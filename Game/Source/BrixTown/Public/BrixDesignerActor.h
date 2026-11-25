#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixDesigner.h"
#include "BrixDesignerActor.generated.h"

class SBox;
class SScrollBox;
class SWidget;

// The character designer (CHR-001, CHR-002, CHR-013; F-CHR-DESIGN level 2), as the slice's life.js openDesigner() and
// buildDesigner(): a new game opens it, a card down the left in the Lamplight look (ADR-0049) with the name, the body
// (height, build, skin), the face (width, eyes, brows, nose, mouth, eye colour), the hair (style, colour), the clothes
// (a top, a bottom, shoes, each in a colour of the palette), the extras (scarf, cap, apron, bag, moustache) and the walk.
// Every choice comes from World/rules/designer.json, and the character is shown live beside it, the camera close round
// them (drag to turn them). Begin starts the game as that character. The clock stands still while it is open.
// The character is the designer's values and nothing else (BrixWorld/BrixDesigner): BrixDesignLook makes their look.
// For test shots: -BrixDesigner opens it (a test shot otherwise starts as the default character, as the slice's ?auto=1),
// -BrixDesignSet=<key>:<value>;... sets values first (hair:afro;skin:7;height:1.8;cap:terra), -BrixDesignSpin=<deg> turns them,
// -BrixDesignBegin presses Begin after 3 s (the game's own -BrixShot is then taken in play).
UCLASS(Config = Game)
class BRIXTOWN_API ABrixDesigner : public AActor
{
	GENERATED_BODY()

public:
	ABrixDesigner();
	void Start(const FString& InCityName);
	virtual void Tick(float DeltaSeconds) override;
	bool IsOpen() const { return bOpen; }
	const FBrixDesign& Design() const { return Look; }
	const FBrixDesignerRules& Rules() const { return D; }
	// what the player wears now changes (the wardrobe, CHR-014): the character is drawn so at once
	void SetDesign(const FBrixDesign& V);
	// the designer closed and the game begins, as this character (its name for the talks, CHR-001)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBegin, const FBrixDesign&);
	FOnBegin OnBegin;
	bool SetValue(const FString& Key, const FString& Value);   // one of -BrixDesignSet's key:value, as the designer would set it

	UPROPERTY(Config) float PanelWidth = 440.f;      // px at 1600 x 900
	UPROPERTY(Config) float SpinPerPixel = .3f;      // degrees the character turns for a pixel dragged
	UPROPERTY(Config) float ShotSeconds = 8.f;       // -BrixDesigner: real seconds after it opens that the test shot is taken

private:
	void Open();
	void Close();
	void Build();
	void Apply();
	void Surprise();
	TSharedRef<SWidget> Foot();
	class ABrixCharacter* Me() const;

	FBrixDesignerRules D;
	FBrixDesign Look;
	bool bOpen = false, bStarted = false, bReady = false;
	double Spin = 0;
	bool bDrag = false;
	FVector2D DragAt = FVector2D::ZeroVector;
	double ShotAt = -1, BeginAt = -1;
	FString CityName, Msg;
	TSharedPtr<SWidget> Root;
	TSharedPtr<SBox> Body, FootBox;
	TSharedPtr<SScrollBox> Scroll;
};
