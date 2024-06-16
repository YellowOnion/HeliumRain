#pragma once

#include "../Flare.h"
#include "../UI/FlareUITypes.h"
#include "GameFramework/HUD.h"
#include "FlareMenuManager.generated.h"


/*----------------------------------------------------
	Definitions
----------------------------------------------------*/

// Widgets
class SFlareTooltip;
class SFlareNotifier;
class SFlareMainOverlay;
class SFlareSpacecraftInfo;
class SFlareConfirmationOverlay;
class SFlareSpacecraftOrderOverlay;
class SFlareShipyardOverlay;

// Menus
class SFlareMainMenu;
class SFlareSettingsMenu;
class SFlareNewGameMenu;
class SFlareStoryMenu;
class SFlareShipMenu;
class SFlareFleetMenu;
class SFlareQuestMenu;
class SFlareOrbitalMenu;
class SFlareLeaderboardMenu;
class SFlareCompanyMenu;
class SFlareSectorMenu;
class SFlareTradeMenu;
class SFlareTradeRouteMenu;
class SFlareWhiteListMenu;
class SFlareCreditsMenu;
class SFlareHelpMenu;
class SFlareEULAMenu;
class SFlareResourcePricesMenu;
class SFlareWorldEconomyMenu;
class SFlareGameOverMenu;
class SFlareTechnologyMenu;
class SFlareSkirmishSetupMenu;
class SFlareSkirmishScoreMenu;

// Gameplay classes
class AFlarePlayerController;
class AFlareGame;

// Menu state
typedef TPair<EFlareMenu::Type, FFlareMenuParameterData> TFlareMenuData;


/*----------------------------------------------------
	Menu manager code
----------------------------------------------------*/

/** The menu manager is the central UI class for the game, controlling fading, menu transitions and setup */
UCLASS()
class HELIUMRAIN_API AFlareMenuManager : public AHUD
{
public:

	GENERATED_UCLASS_BODY()

public:
		
	/*----------------------------------------------------
		Setup and engine API
	----------------------------------------------------*/
		
	/** Construct the Slate menu interface */
	void SetupMenu();

	void Tick(float DeltaSeconds) override;


	/*----------------------------------------------------
		Public API for interaction
	----------------------------------------------------*/
	
	/** Open the main overlay */
	void OpenMainOverlay();

	/** Close the main overlay */
	void CloseMainOverlay();

	/** Open target menu if not open, close otherwise */
	bool ToggleMenu(EFlareMenu::Type Target);

	/** Asynchronously switch to a target menu, with optional data */
	bool OpenMenu(EFlareMenu::Type Target, FFlareMenuParameterData Data = FFlareMenuParameterData(), bool AddToHistory = true, bool OpenDirectly = false, bool UpdateQuestManager = true);
	
	/** Close the current menu */
	void CloseMenu(bool HardClose = false);

	/** Show the list of spacecraft that can be ordered here */
	void OpenSpacecraftOrder(FFlareMenuParameterData Data, FOrderDelegate ConfirmationCallback);

	/** Show the list of shipyards */
	void OpenShipyardOrder();

	/** Return to the previous menu */
	void Back();

	/** Reload the current menu with the same parameters */
	void Reload();

	/** Remove the history */
	void ClearHistory();

	/** Show a notification to the user */
	bool Notify(FText Text, FText Info, FName Tag, EFlareNotification::Type Type, bool Pinned = false, EFlareMenu::Type TargetMenu = EFlareMenu::MENU_None, FFlareMenuParameterData TargetInfo = FFlareMenuParameterData());

	/** Remove all notifications with the given tag */
	void ClearNotifications(FName Tag);

	/** Remove all notifications from the screen */
	void FlushNotifications();

	/** Show the confirmation overlay */
	void Confirm(FText Title, FText Text, FSimpleDelegate OnConfirmed, FSimpleDelegate OnCancel = FSimpleDelegate(), FSimpleDelegate OnIgnore = FSimpleDelegate());

	/** Start displaying the tooltip */
	void ShowTooltip(SWidget* TargetWidget, FText Title, FText Content);

	/** Stop displaying the tooltip */
	void HideTooltip(SWidget* TargetWidget);

	/** Register as the latest spacecraft info */
	void RegisterSpacecraftInfo(SFlareSpacecraftInfo* Info);

	/** Unregister as the latest spacecraft info */
	void UnregisterSpacecraftInfo(SFlareSpacecraftInfo* Info);

	/** Spacecraft info hotkey was pressed */
	void SpacecraftInfoHotkey(int32 Index);

	/** Focus navigation with joystick */
	void JoystickCursorMove(FVector2D Move);

	/** Skirmish is done */
	void PrepareSkirmishEnd();

protected:

	/*----------------------------------------------------
		Internal management
	----------------------------------------------------*/

	/** Hide the menu */
	void ResetMenu();

	/** Fade from black */
	void FadeIn();

	/** Fade to black */
	void FadeOut();
	
	/** After a fading process has completed, proceed */
	void ProcessNextMenu();

	/** Target menu was correctly entered */
	void OnEnterMenu(bool LightBackground = true, bool ShowOverlay = true, bool TellPlayer = true);

	/** Start using the light background setting */
	void UseLightBackground();

	/** Start using the dark background setting */
	void UseDarkBackground();


	/*----------------------------------------------------
		Internal menu callbacks
	----------------------------------------------------*/

	/** Create the game */
	bool CreateGame();

	/** Load the game */
	bool LoadGame();

	/** Fly this ship */
	bool FlyShip();

	/** Travel here */
	void Travel();

	/** Reload the sector */
	bool ReloadSector();

	/** Game over */
	void GameOver();

	/** Reload the sector after a FF */
	bool FastForwardSingle();

	/** Open the main menu */
	void OpenMainMenu();

	/** Open the settings menu */
	void OpenSettingsMenu();

	/** Open the new game menu */
	void OpenNewGameMenu();

	/** Open the story menu */
	void OpenStoryMenu();

	/** Open the company menu */
	void InspectCompany();

	/** Show the config menu for a specific ship */
	void InspectShip(bool IsEditable = false);

	/** Show the fleet menu */
	void OpenFleetMenu();

	/** Show the quest menu */
	void OpenQuestMenu();

	/** Open the sector menu */
	void OpenSector();

	/** Open the trade menu */
	void OpenTrade();

	/** Open the trade route menu */
	void OpenTradeRoute();

	void OpenWhitelist();

	/** Open the orbital menu */
	void OpenOrbit();

	/** Open the company menu */
	void OpenLeaderboard();

	/** Open the resource prices menu */
	void OpenResourcePrices();

	/** Open the world economy menu */
	void OpenWorldEconomy();

	/** Open the technology menu */
	void OpenTechnology();

	/** Go to the game's credits */
	void OpenCredits();

	/** Go to the game's in game help menu */
	void OpenHelp();

	/** Go to the game's EULA */
	void OpenEULA();

	/** Go to the new skirmish menu */
	void OpenSkirmishSetup();

	/** Go to the skirmish end-game score menu */
	void OpenSkirmishScore();

	/** Exit the menu */
	void ExitMenu();


public:

	/*----------------------------------------------------
		Getters
	----------------------------------------------------*/

	/** Get the name text for this menu */
	static FText GetMenuName(EFlareMenu::Type MenuType, bool Uppercase = false);

	/** Get the Slate icon brush for this menu */
	static const FSlateBrush* GetMenuIcon(EFlareMenu::Type MenuType);

	/** Get the key bound to this menu */
	static FString GetMenuKey(EFlareMenu::Type MenuType);

	/** Get the key bound to this action */
	static FString GetKeyNameFromActionName(FName ActionName);

	/** Is UI visible */
	UFUNCTION(BlueprintCallable, Category = "Flare")
	bool IsUIOpen() const;

	/** Is a menu open */
	UFUNCTION(BlueprintCallable, Category = "Flare")
	bool IsMenuOpen() const;

	/** Can we go back ? */
	bool HasPreviousMenu() const;

	/** Is the overlay open ? */
	UFUNCTION(BlueprintCallable, Category = "Flare")
	bool IsOverlayOpen() const;

	/** Is a menu being opened or closed */
	UFUNCTION(BlueprintCallable, Category = "Flare")
	bool IsSwitchingMenu() const;

	/** Are we during a fade transition ? */
	UFUNCTION(BlueprintCallable, Category = "Flare")
	bool IsFading() const;

	/** Are we during a fade transition ? */
	UFUNCTION(BlueprintCallable, Category = "Flare")
	bool IsFadingFromBlack() const;

	/** Get the fade value */
	UFUNCTION(BlueprintCallable, Category = "Flare")
	float GetFadeAlpha() const;

	/** Which menu, if any, is opened ? */
	UFUNCTION(BlueprintCallable, Category = "Flare")
	EFlareMenu::Type GetCurrentMenu() const;

	/** Which menu, if any, was opened ? */
	UFUNCTION(BlueprintCallable, Category = "Flare")
	EFlareMenu::Type GetPreviousMenu() const;

	/** Which menu, if any, is coming next ? */
	UFUNCTION(BlueprintCallable, Category = "Flare")
	EFlareMenu::Type GetNextMenu() const;

	/** Get the PC */
	AFlarePlayerController* GetPC() const;

	/** Get the game */
	AFlareGame* GetGame() const;

	/** Get the spacecraft menu */
	TSharedPtr<SFlareShipMenu> GetShipMenu() const;

	UFlareSimulatedSpacecraft* GetShipMenuTergetSpacecraft() const;

	/** Get the Trade menu */
	TSharedPtr<SFlareTradeMenu> GetTradeMenu() const;

	/** Get the Leaderboard/Diplomacy menu */
	TSharedPtr<SFlareLeaderboardMenu> GetLeaderboardMenu() const;
	
	/** Get the fading duration */
	inline float GetFadeDuration() const
	{
		return FadeDuration;
	}

	/** Get orbit menu */
	TSharedPtr<SFlareOrbitalMenu> GetOrbitMenu()
	{
		return OrbitMenu;
	}

	void UpdateOrbitMenuFleets();

	/** Get sector menu */
	TSharedPtr<SFlareSectorMenu> GetSectorMenu()
	{
		return SectorMenu;
	}

	/** Get technology menu */
	TSharedPtr<SFlareTechnologyMenu> GetTechnologyMenu()
	{
		return TechnologyMenu;
	}

	/** Get fleet menu */
	TSharedPtr<SFlareFleetMenu> GetFleetMenu()
	{
		return FleetMenu;
	}

	/** Get notifier */
	TSharedPtr<SFlareNotifier> GetNotifier()
	{
		return Notifier;
	}

	/** Get the joystick position */
	FVector2D GetJoystickCursorPosition() const
	{
		return JoystickCursorPosition;
	}

	/** Get the height of the main overlay */
	static int32 GetMainOverlayHeight();

	/** Get the skirmish end countdown */
	int32 GetSkirmishCountdown() const;

	/** Get the menu manager */
	static AFlareMenuManager* GetSingleton();

protected:

	/*----------------------------------------------------
		Protected data
	----------------------------------------------------*/
	
	// Singleton pointer, because we reference this class from the entire world
	static AFlareMenuManager*               Singleton;

	// General data
	bool                                    MenuOperationDone;
	bool                                    MenuIsOpen;
	bool                                    FadeFromBlack;
	bool                                    SkipNextFade;
	bool                                    NotifyExitSector;
	float                                   FadeDuration;
	float                                   FadeTimer;
	float                                   SkirmishCountdownDuration;
	float                                   SkirmishCountdownTimer;
	TFlareMenuData                          CurrentMenu;
	TFlareMenuData                          NextMenu;
	TArray<TFlareMenuData>                  MenuHistory;
	SFlareSpacecraftInfo*                   CurrentSpacecraftInfo;
	FVector2D                               JoystickCursorPosition;

	// Menu tools
	TSharedPtr<SBorder>                     Fader;
	TSharedPtr<SFlareTooltip>               Tooltip;
	TSharedPtr<SFlareNotifier>              Notifier;
	TSharedPtr<SFlareMainOverlay>           MainOverlay;
	TSharedPtr<SFlareConfirmationOverlay>   Confirmation;
	TSharedPtr<SFlareSpacecraftOrderOverlay>SpacecraftOrder;
	TSharedPtr<SFlareShipyardOverlay>       ShipyardOrder;

	// Menus
	TSharedPtr<SFlareMainMenu>              MainMenu;
	TSharedPtr<SFlareSettingsMenu>          SettingsMenu;
	TSharedPtr<SFlareNewGameMenu>           NewGameMenu;
	TSharedPtr<SFlareStoryMenu>             StoryMenu;
	TSharedPtr<SFlareShipMenu>              ShipMenu;
	TSharedPtr<SFlareFleetMenu>             FleetMenu;
	TSharedPtr<SFlareQuestMenu>             QuestMenu;
	TSharedPtr<SFlareOrbitalMenu>           OrbitMenu;
	TSharedPtr<SFlareLeaderboardMenu>       LeaderboardMenu;
	TSharedPtr<SFlareCompanyMenu>           CompanyMenu;
	TSharedPtr<SFlareSectorMenu>            SectorMenu;
	TSharedPtr<SFlareTradeMenu>             TradeMenu;
	TSharedPtr<SFlareTradeRouteMenu>        TradeRouteMenu;
	TSharedPtr<SFlareWhiteListMenu>         CompanyWhiteListMenu;
	TSharedPtr<SFlareCreditsMenu>           CreditsMenu;
	TSharedPtr<SFlareHelpMenu>	            HelpMenu;

	TSharedPtr<SFlareEULAMenu>              EULAMenu;
	TSharedPtr<SFlareResourcePricesMenu>    ResourcePricesMenu;
	TSharedPtr<SFlareWorldEconomyMenu>      WorldEconomyMenu;
	TSharedPtr<SFlareTechnologyMenu>        TechnologyMenu;
	TSharedPtr<SFlareGameOverMenu>          GameOverMenu;
	TSharedPtr<SFlareSkirmishSetupMenu>     SkirmishSetupMenu;
	TSharedPtr<SFlareSkirmishScoreMenu>     SkirmishScoreMenu;
	TArray<FString>							ActiveModStrings;
};
