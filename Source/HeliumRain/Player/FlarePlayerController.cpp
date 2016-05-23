
#include "../Flare.h"
#include "FlarePlayerController.h"
#include "../Game/FlareGameTools.h"
#include "../Spacecrafts/FlareSpacecraft.h"
#include "../Game/Planetarium/FlareSimulatedPlanetarium.h"
#include "../Game/FlareGameUserSettings.h"
#include "../Game/AI/FlareCompanyAI.h"
#include "FlareMenuManager.h"
#include "EngineUtils.h"


#define LOCTEXT_NAMESPACE "AFlarePlayerController"


/*----------------------------------------------------
	Constructor
----------------------------------------------------*/

AFlarePlayerController::AFlarePlayerController(const class FObjectInitializer& PCIP)
	: Super(PCIP)
	, DustEffect(NULL)
	, Company(NULL)
	, WeaponSwitchTime(10.0f)
	, TimeSinceWeaponSwitch(0)
{
	CheatClass = UFlareGameTools::StaticClass();

	// Sounds
	static ConstructorHelpers::FObjectFinder<USoundCue> OnSoundObj(TEXT("/Game/Master/Sound/A_Beep_On"));
	static ConstructorHelpers::FObjectFinder<USoundCue> OffSoundObj(TEXT("/Game/Master/Sound/A_Beep_Off"));
	OnSound = OnSoundObj.Object;
	OffSound = OffSoundObj.Object;
	
	// Mouse
	static ConstructorHelpers::FObjectFinder<UParticleSystem> DustEffectTemplateObj(TEXT("/Game/Master/Particles/PS_Dust"));
	DustEffectTemplate = DustEffectTemplateObj.Object;
	DefaultMouseCursor = EMouseCursor::Default;
	
	// Gameplay
	QuickSwitchNextOffset = 0;
	CurrentObjective.Set = false;
	CurrentObjective.Version = 0;
	IsTest1 = false;
	IsTest2 = false;
	KeepFlyingInMenus = true;
	LastBattleState = EFlareSectorBattleState::NoBattle;
}


/*----------------------------------------------------
	Gameplay
----------------------------------------------------*/

void AFlarePlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnableCheats();

	// Settings
	UFlareGameUserSettings* MyGameSettings = Cast<UFlareGameUserSettings>(GEngine->GetGameUserSettings());
	check(MyGameSettings);
	UseDarkThemeForStrategy = MyGameSettings->UseDarkThemeForStrategy;
	UseDarkThemeForNavigation = MyGameSettings->UseDarkThemeForNavigation;
	UseCockpit = MyGameSettings->UseCockpit;
	MusicVolume = MyGameSettings->MusicVolume;
	MasterVolume = MyGameSettings->MasterVolume;

	// Cockpit
	SetupCockpit();
	CockpitManager->SetupCockpit(this);
	PlayerCameraManager->SetFOV(92);

	// Menu manager
	SetupMenu();
	MenuManager->OpenMenu(EFlareMenu::MENU_Main);

	// Sound manager
	SoundManager = NewObject<UFlareSoundManager>(this, UFlareSoundManager::StaticClass());
	if (SoundManager)
	{
		SoundManager->Setup(this);
		SoundManager->RequestMusicTrack(EFlareMusicTrack::MainMenu);
		SoundManager->SetMusicVolume(MusicVolume);
		SoundManager->SetMasterVolume(MasterVolume);
	}
}

void AFlarePlayerController::PlayerTick(float DeltaSeconds)
{
	Super::PlayerTick(DeltaSeconds);
	AFlareHUD* HUD = GetNavHUD();
	TimeSinceWeaponSwitch += DeltaSeconds;

	// We are flying
	if (ShipPawn)
	{
		HUD->SetInteractive(ShipPawn->GetStateManager()->IsWantContextMenu());
		
		UFlareSector* Sector = Cast<UFlareSector>(ShipPawn->GetCurrentSectorInterface());
		GetCompany()->GetAI()->ResetControlGroups(Sector);


		// Battle state
		if (GetGame()->GetActiveSector())
		{
			EFlareSectorBattleState::Type BattleState = GetGame()->GetActiveSector()->GetSectorBattleState(GetCompany());
			if (BattleState != LastBattleState)
			{
				LastBattleState = BattleState;
				OnBattleStateChanged(BattleState);
			}
		}

		// FLIR Debug Code. Keep it for future ship setup
		/*TArray<FName> SocketNames  = ShipPawn->Airframe->GetAllSocketNames();
		for (int32 SocketIndex = 0; SocketIndex < SocketNames.Num(); SocketIndex++)
		{
			FLOGV("Check socket = %s", *SocketNames[SocketIndex].ToString());
			if (SocketNames[SocketIndex] == "Dock" || SocketNames[SocketIndex].ToString().StartsWith("FLIR"))
			{
				FTransform CameraWorldTransform = ShipPawn->Airframe->GetSocketTransform(SocketNames[SocketIndex]);

				FVector CameraLocation = CameraWorldTransform.GetTranslation();
				FVector CandidateCameraMainDirection = CameraWorldTransform.GetRotation().RotateVector(FVector(1,0,0));

				DrawDebugLine(GetWorld(), CameraLocation, CameraLocation + 500 * CandidateCameraMainDirection, FColor::Red, false);
				DrawDebugSphere(GetWorld(), CameraLocation, 50, 32, FColor::Green, false);
			}
		}*/
	}

	// Mouse cursor
	bool NewShowMouseCursor = true;
	if (!MenuManager->IsMenuOpen() && ShipPawn && !ShipPawn->GetStateManager()->IsWantCursor())
	{
		NewShowMouseCursor = false;
	}

	if (NewShowMouseCursor != bShowMouseCursor)
	{
		// Set the mouse status
		FLOGV("AFlarePlayerController::PlayerTick : New mouse cursor state is %d", NewShowMouseCursor);
		bShowMouseCursor = NewShowMouseCursor;

		ResetMousePosition();

		// Force focus to UI
		if (NewShowMouseCursor)
		{
			FInputModeGameAndUI InputMode;
			SetInputMode(InputMode);

			if (!NewShowMouseCursor)
			{
				ULocalPlayer* LocalPlayer = Cast< ULocalPlayer >( Player );

				UGameViewportClient* GameViewportClient = GetWorld()->GetGameViewport();
				TSharedPtr<SViewport> ViewportWidget = GameViewportClient->GetGameViewportWidget();
				if (ViewportWidget.IsValid())
				{
					TSharedRef<SViewport> ViewportWidgetRef = ViewportWidget.ToSharedRef();
					LocalPlayer->GetSlateOperations().UseHighPrecisionMouseMovement(ViewportWidgetRef);
				}
			}
		}

		// Force focus to game
		else
		{
			FInputModeGameOnly InputMode;
			SetInputMode(InputMode);
		}
	}

	// Spawn dust effects if they are not already here
	if (!DustEffect && ShipPawn)
	{
		DustEffect = UGameplayStatics::SpawnEmitterAtLocation(this, DustEffectTemplate, FVector::ZeroVector);
	}

	// Update dust effects
	if (DustEffect && ShipPawn && !IsInMenu())
	{
		// Ship velocity
		FVector Velocity = ShipPawn->GetLinearVelocity();
		FVector Direction = Velocity;
		Direction.Normalize();

		// Particle position
		FVector ViewLocation;
		FRotator ViewRotation;
		GetPlayerViewPoint(ViewLocation, ViewRotation);
		ViewLocation += Direction.Rotation().RotateVector(5000 * FVector(1, 0, 0));
		DustEffect->SetWorldLocation(ViewLocation);

		// Particle params
		float VelocityFactor = FMath::Clamp(Velocity.Size() / 100.0f, 0.0f, 1.0f);
		FLinearColor Color = FLinearColor::White * VelocityFactor;
		DustEffect->SetFloatParameter("SpawnCount", VelocityFactor);
		DustEffect->SetColorParameter("Intensity", Color);
		DustEffect->SetVectorParameter("Direction", -Direction);
		DustEffect->SetVectorParameter("Size", FVector(1, VelocityFactor, 1));
	}

	// Sound
	if (SoundManager)
	{
		SoundManager->Update(DeltaSeconds);
	}
}

void AFlarePlayerController::SetExternalCamera(bool NewState)
{
	if (ShipPawn)
	{
		// No internal camera when docked
		if (ShipPawn && ShipPawn->GetNavigationSystem()->IsDocked())
		{
			NewState = true;
		}

		// Send the camera order to the ship
		ShipPawn->GetStateManager()->SetExternalCamera(NewState);
	}
}

void AFlarePlayerController::FlyShip(AFlareSpacecraft* Ship, bool PossessNow)
{
	// Reset the current ship to auto
	if (ShipPawn)
	{
		ShipPawn->ResetCurrentTarget();
		ShipPawn->GetStateManager()->EnablePilot(true);
	}

	// Fly the new ship
	if (PossessNow)
	{
		Ship->ResetCurrentTarget();
		Possess(Ship);
	}

	// Setup everything
	ShipPawn = Ship;
	SetExternalCamera(false);
	ShipPawn->GetStateManager()->EnablePilot(false);
	ShipPawn->GetWeaponsSystem()->DeactivateWeapons();
	CockpitManager->OnFlyShip(ShipPawn);

	// Combat groups
	GetCompany()->GetAI()->SetCurrentShipGroup(EFlareCombatGroup::AllMilitary);
	GetCompany()->GetAI()->ResetShipGroup(EFlareCombatTactic::ProtectMe);

	// Inform the player
	if (Ship)
	{
		// Count owned ships
		int32 OwnedSpacecraftCount = 0;
		for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
		{
			AFlareSpacecraft* OtherSpacecraft = Cast<AFlareSpacecraft>(*Iterator);
			if (OtherSpacecraft && OtherSpacecraft->GetCompany() == GetCompany())
			{
				OwnedSpacecraftCount++;
			}
		}

		// Notification
		FText Text = FText::Format(LOCTEXT("FlyingFormat", "Now flying {0}"), FText::FromName(Ship->GetImmatriculation()));
		FText Info = (OwnedSpacecraftCount > 1) ? LOCTEXT("FlyingInfo", "You can switch to nearby ships with N.") : FText();
		Notify(Text, Info, "flying-info", EFlareNotification::NT_Info);

		// HUD update
		GetNavHUD()->OnTargetShipChanged();
		SetSelectingWeapon();

		GetGame()->GetQuestManager()->OnFlyShip(Ship);
	}
}

void AFlarePlayerController::PrepareForExit()
{
	if (IsInMenu())
	{
		MenuManager->CloseMenu(true);
	}
}


/*----------------------------------------------------
	Data management
----------------------------------------------------*/

void AFlarePlayerController::SetCompanyDescription(const FFlareCompanyDescription& SaveCompanyData)
{
	CompanyData = SaveCompanyData;
}

void AFlarePlayerController::Load(const FFlarePlayerSave& SavePlayerData)
{
	PlayerData = SavePlayerData;
	SelectedFleet = GetGame()->GetGameWorld()->FindFleet(PlayerData.SelectedFleetIdentifier);
	Company = GetGame()->GetGameWorld()->FindCompany(PlayerData.CompanyIdentifier);
}

void AFlarePlayerController::OnLoadComplete()
{
	SetWorldPause(true);
	Company->UpdateCompanyCustomization();
}

void AFlarePlayerController::OnSectorActivated(UFlareSector* ActiveSector)
{
	FLOGV("AFlarePlayerController::OnSectorActivated LastFlownShip=%s", *ActiveSector->GetData()->LastFlownShip.ToString());
	bool CandidateFound = false;

	// Last flown ship
	if (ActiveSector->GetData()->LastFlownShip != "")
	{
		FLOG("AFlarePlayerController::OnSectorActivated not null last ship");
		AFlareSpacecraft* Candidate = ActiveSector->FindSpacecraft(ActiveSector->GetData()->LastFlownShip);
		if (Candidate)
		{
			FLOG("AFlarePlayerController::OnSectorActivated last ship found");
			CandidateFound = true;

			// Disable pilot during the switch
			Candidate->GetStateManager()->EnablePilot(false);
			FlyShip(Candidate);
		}
	}

	if (!CandidateFound)
	{
		FLOG("AFlarePlayerController::OnSectorActivated no candidate");
		QuickSwitch();
	}

	// Level music
	EFlareMusicTrack::Type LevelMusic = ActiveSector->GetDescription()->LevelTrack;
	if (LevelMusic != EFlareMusicTrack::None)
	{
		SoundManager->RequestMusicTrack(LevelMusic);
	}
	else
	{
		SoundManager->RequestMusicTrack(EFlareMusicTrack::Exploration);
	}
}

void AFlarePlayerController::OnSectorDeactivated()
{
	FLOG("AFlarePlayerController::OnSectorDeactivated");

	// Reset the ship
	CockpitManager->OnStopFlying();
	if (ShipPawn)
	{
		ShipPawn->ResetCurrentTarget();
		ShipPawn = NULL;
	}

	// Reset the HUD
	if (GetNavHUD())
	{
		GetNavHUD()->RemoveAllTargets();
	}

	// Reset states
	LastBattleState = EFlareSectorBattleState::NoBattle;
	MenuManager->OpenMenu(EFlareMenu::MENU_Orbit);
	SoundManager->RequestMusicTrack(EFlareMusicTrack::MainMenu);
}

void AFlarePlayerController::OnBattleStateChanged(EFlareSectorBattleState::Type NewBattleState)
{
	FLOG("AFlarePlayerController::OnBattleStateChanged");

	if (NewBattleState == EFlareSectorBattleState::NoBattle)
	{
		FLOG("AFlarePlayerController::OnBattleStateChanged : peace");
		SoundManager->RequestMusicTrack(EFlareMusicTrack::Exploration);
	}
	else
	{
		FLOG("AFlarePlayerController::OnBattleStateChanged : battle");
		SoundManager->RequestMusicTrack(EFlareMusicTrack::Combat);
	}
}

void AFlarePlayerController::Save(FFlarePlayerSave& SavePlayerData, FFlareCompanyDescription& SaveCompanyData)
{
	if (SelectedFleet)
	{
		PlayerData.SelectedFleetIdentifier = SelectedFleet->GetIdentifier();
	}
	else
	{
		PlayerData.SelectedFleetIdentifier = NAME_None;
	}
	SavePlayerData = PlayerData;
	SaveCompanyData = CompanyData;
}

void AFlarePlayerController::SetCompany(UFlareCompany* NewCompany)
{
	Company = NewCompany;
}

void AFlarePlayerController::SetLastFlownShip(FName LastFlownShipIdentifier)
{
	PlayerData.LastFlownShipIdentifier = LastFlownShipIdentifier;
}


/*----------------------------------------------------
	Menus
----------------------------------------------------*/

void AFlarePlayerController::Notify(FText Title, FText Info, FName Tag, EFlareNotification::Type Type, float Timeout, EFlareMenu::Type TargetMenu, void* TargetInfo, FName TargetSpacecraft)
{
	FLOGV("AFlarePlayerController::Notify : '%s'", *Title.ToString());
	MenuManager->Notify(Title, Info, Tag, Type, Timeout, TargetMenu, TargetInfo, TargetSpacecraft);
}

void AFlarePlayerController::SetupCockpit()
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = Instigator;
	SpawnInfo.ObjectFlags |= RF_Transient;
	CockpitManager = GetWorld()->SpawnActor<AFlareCockpitManager>(AFlareCockpitManager::StaticClass(), SpawnInfo);
}

void AFlarePlayerController::SetupMenu()
{
	// Save the default pawn
	APawn* DefaultPawn = GetPawn();

	// Spawn the menu pawn at an arbitrarily large location
	FVector SpawnLocation(1000000 * FVector(-1, -1, -1));
	MenuPawn = GetWorld()->SpawnActor<AFlareMenuPawn>(GetGame()->GetMenuPawnClass(), SpawnLocation, FRotator::ZeroRotator);
	Possess(MenuPawn);
	
	// Spawn the menu manager
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.Instigator = Instigator;
	SpawnInfo.ObjectFlags |= RF_Transient;
	MenuManager = GetWorld()->SpawnActor<AFlareMenuManager>(AFlareMenuManager::StaticClass(), SpawnInfo);

	// Setup menus and HUD
	MenuManager->SetupMenu();
	GetNavHUD()->Setup(MenuManager);

	// Destroy the old pawn
	if (DefaultPawn)
	{
		DefaultPawn->Destroy();
	}
	GetNavHUD()->UpdateHUDVisibility();
}

void AFlarePlayerController::UpdateMenuTheme()
{
	if (GetGame()->GetActiveSector())
	{
		if (UseDarkThemeForNavigation)
			MenuManager->UseDarkBackground();
		else
			MenuManager->UseLightBackground();
	}
	else
	{
		if (UseDarkThemeForStrategy)
			MenuManager->UseDarkBackground();
		else
			MenuManager->UseLightBackground();
	}
}

void AFlarePlayerController::OnEnterMenu()
{
	if (!IsInMenu())
	{
		ClientPlaySound(OnSound);

		// Shutdown trails, etc
		if (KeepFlyingInMenus)
		{
			if (DustEffect)
			{
				DustEffect->Deactivate();
			}
		}

		// Possess the menu pawn
		else
		{
			Possess(MenuPawn);
			MenuPawn->SetActorHiddenInGame(false);
			SetWorldPause(true);
		}
	}

	// Update the HUD
	if (GetNavHUD())
	{
		GetNavHUD()->UpdateHUDVisibility();
	}
}

void AFlarePlayerController::OnExitMenu()
{
	if (IsInMenu())
	{
		ClientPlaySound(OffSound);

		// The player ship is already possessed
		if (KeepFlyingInMenus)
		{
			if (DustEffect)
			{
				DustEffect->Activate();
			}
		}

		// Possess the player ship
		else
		{
			MenuPawn->SetActorHiddenInGame(true);
			SetWorldPause(false);
			Possess(ShipPawn);
			GetNavHUD()->OnTargetShipChanged();
		}
		
		CockpitManager->OnFlyShip(ShipPawn);
		SetSelectingWeapon();
	}

	// Update the HUD
	if (GetNavHUD())
	{
		GetNavHUD()->UpdateHUDVisibility();
	}
}

void AFlarePlayerController::SetWorldPause(bool Pause)
{
	FLOGV("AFlarePlayerController::SetWorldPause world %d", Pause);

	if (GetGame()->GetActiveSector())
	{
		GetGame()->GetActiveSector()->SetPause(Pause);
	}
}

void AFlarePlayerController::SelectFleet(UFlareFleet* Fleet)
{
	if (Fleet == NULL)
	{
		FLOG("Select no fleet");
	}
	else
	{
		FLOGV("Select fleet %s : %s", *Fleet->GetIdentifier().ToString(), *Fleet->GetFleetName().ToString());
	}
	SelectedFleet = Fleet;
}

UFlareFleet* AFlarePlayerController::GetSelectedFleet()
{
	return SelectedFleet;
}


bool AFlarePlayerController::IsInMenu()
{
	return MenuManager->IsMenuOpen();
}

FVector2D AFlarePlayerController::GetMousePosition()
{
	FVector2D Result;
	ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(Player);

	if (LocalPlayer && LocalPlayer->ViewportClient)
	{
		FVector2D ScreenPosition;
		LocalPlayer->ViewportClient->GetMousePosition(Result);
	}

	return Result;
}

void AFlarePlayerController::ResetMousePosition()
{
	auto& App = FSlateApplication::Get();
	FVector2D CursorPos = App.GetCursorPos();
	App.SetCursorPos(CursorPos + FVector2D(0, 1));
	App.OnMouseMove();
	App.SetCursorPos(CursorPos);
	App.OnMouseMove();
	App.SetAllUserFocusToGameViewport();
}

void AFlarePlayerController::SetSelectingWeapon()
{
	TimeSinceWeaponSwitch = 0;
}

bool AFlarePlayerController::IsSelectingWeapon() const
{
	return (TimeSinceWeaponSwitch < WeaponSwitchTime);
}

void AFlarePlayerController::NotifyDockingResult(bool Success, IFlareSpacecraftInterface* Target)
{
	if (Success)
	{
		Notify(
			LOCTEXT("DockingGranted", "Docking granted"),
			FText::Format(
				LOCTEXT("DockingGrantedInfoFormat", "Your ship is now automatically docking at {0}. Using manual controls will abort docking."),
				FText::FromName(Target->GetImmatriculation())),
			"docking-granted",
			EFlareNotification::NT_Info,
			10.0f);
	}
	else
	{
		Notify(
			LOCTEXT("DockingDenied", "Docking denied"),
			FText::Format(LOCTEXT("DockingDeniedInfoFormat", "{0} denied your docking request"), FText::FromName(Target->GetImmatriculation())),
			"docking-denied",
			EFlareNotification::NT_Info,
			10.0f);
	}
}


/*----------------------------------------------------
	Objectives
----------------------------------------------------*/

void AFlarePlayerController::StartObjective(FText Name, FFlarePlayerObjectiveData Data)
{
	if (!CurrentObjective.Set)
	{
		CurrentObjective.Set = true;
		CurrentObjective.Version++;
	}

	CurrentObjective.Data = Data;
}

void AFlarePlayerController::CompleteObjective()
{
	CurrentObjective.Set = false;
}

bool AFlarePlayerController::HasObjective() const
{
	return CurrentObjective.Set;
}

const FFlarePlayerObjective* AFlarePlayerController::GetCurrentObjective() const
{
	return (CurrentObjective.Set? &CurrentObjective : NULL);
}



/*----------------------------------------------------
	Customization
----------------------------------------------------*/

void AFlarePlayerController::SetBasePaintColorIndex(int32 Index)
{
	CompanyData.CustomizationBasePaintColorIndex = Index;
	Company->UpdateCompanyCustomization();
}

void AFlarePlayerController::SetPaintColorIndex(int32 Index)
{
	CompanyData.CustomizationPaintColorIndex = Index;
	Company->UpdateCompanyCustomization();
}

void AFlarePlayerController::SetOverlayColorIndex(int32 Index)
{
	CompanyData.CustomizationOverlayColorIndex = Index;
	Company->UpdateCompanyCustomization();
}

void AFlarePlayerController::SetLightColorIndex(int32 Index)
{
	CompanyData.CustomizationLightColorIndex = Index;
	Company->UpdateCompanyCustomization();
}

void AFlarePlayerController::SetPatternIndex(int32 Index)
{
	CompanyData.CustomizationPatternIndex = Index;
	Company->UpdateCompanyCustomization();
}


/*----------------------------------------------------
	Input
----------------------------------------------------*/

void AFlarePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindAction("ToggleCamera", EInputEvent::IE_Released, this, &AFlarePlayerController::ToggleCamera);
	InputComponent->BindAction("ToggleMenu", EInputEvent::IE_Released, this, &AFlarePlayerController::ToggleMenu);
	InputComponent->BindAction("BackMenu", EInputEvent::IE_Released, this, &AFlarePlayerController::BackMenu);
	InputComponent->BindAction("Simulate", EInputEvent::IE_Released, this, &AFlarePlayerController::Simulate);
	InputComponent->BindAction("SettingsMenu", EInputEvent::IE_Released, this, &AFlarePlayerController::SettingsMenu);
	InputComponent->BindAction("ToggleCombat", EInputEvent::IE_Released, this, &AFlarePlayerController::ToggleCombat);
	InputComponent->BindAction("TooglePilot", EInputEvent::IE_Released, this, &AFlarePlayerController::TogglePilot);
	InputComponent->BindAction("ToggleHUD", EInputEvent::IE_Released, this, &AFlarePlayerController::ToggleHUD);
	InputComponent->BindAction("QuickSwitch", EInputEvent::IE_Released, this, &AFlarePlayerController::QuickSwitch);

	InputComponent->BindAction("Wheel", EInputEvent::IE_Pressed, this, &AFlarePlayerController::WheelPressed);
	InputComponent->BindAction("Wheel", EInputEvent::IE_Released, this, &AFlarePlayerController::WheelReleased);

	InputComponent->BindAxis("MouseInputX", this, &AFlarePlayerController::MouseInputX);
	InputComponent->BindAxis("MouseInputY", this, &AFlarePlayerController::MouseInputY);

	InputComponent->BindAction("Test1", EInputEvent::IE_Released, this, &AFlarePlayerController::Test1);
	InputComponent->BindAction("Test2", EInputEvent::IE_Released, this, &AFlarePlayerController::Test2);
}

void AFlarePlayerController::MousePositionInput(FVector2D Val)
{
	if (ShipPawn)
	{
		ShipPawn->GetStateManager()->SetPlayerMousePosition(Val);
	}
}

void AFlarePlayerController::ToggleCamera()
{
	if (ShipPawn)
	{
		SetExternalCamera(!ShipPawn->GetStateManager()->IsExternalCamera());
	}
}

void AFlarePlayerController::ToggleMenu()
{
	if (GetGame()->IsLoadedOrCreated() && GetGame()->GetActiveSector())
	{
		if (IsInMenu())
		{
			MenuManager->CloseMenu();
		}
		else
		{
			MenuManager->OpenMenu(EFlareMenu::MENU_Dashboard);
		}
	}
}

void AFlarePlayerController::BackMenu()
{
	FLOG("AFlarePlayerController::BackMenu");
	if (IsInMenu())
	{
		FLOG("AFlarePlayerController::BackMenu IsInMenu");
		MenuManager->Back();
	}
}

void AFlarePlayerController::Simulate()
{
	if (!GetGame()->IsLoadedOrCreated())
	{
		return;
	}


	UFlareSimulatedSector* LastActiveSector = NULL;

	if(GetGame()->GetActiveSector())
	{
		LastActiveSector = GetGame()->GetActiveSector()->GetSimulatedSector();
		GetGame()->DeactivateSector(this);
	}

	GetGame()->GetGameWorld()->Simulate();

	if(LastActiveSector)
	{
		GetGame()->ActivateSector(this, LastActiveSector);
	}
}

void AFlarePlayerController::SettingsMenu()
{
	if (MenuManager->GetCurrentMenu() != EFlareMenu::MENU_Settings)
	{
		FLOG("AFlarePlayerController::SettingsMenu");
		MenuManager->OpenMenu(EFlareMenu::MENU_Settings);
	}
}

void AFlarePlayerController::ToggleCombat()
{
	if (ShipPawn && ShipPawn->IsMilitary() && !ShipPawn->GetNavigationSystem()->IsDocked() && !IsInMenu())
	{
		FLOG("AFlarePlayerController::ToggleCombat");
		ShipPawn->GetWeaponsSystem()->ToogleWeaponActivation();
		if(ShipPawn->GetWeaponsSystem()->GetActiveWeaponType() == EFlareWeaponGroupType::WG_BOMB || ShipPawn->GetWeaponsSystem()->GetActiveWeaponType() == EFlareWeaponGroupType::WG_GUN)
		{
			SetExternalCamera(false);
		}
	}
}

void AFlarePlayerController::TogglePilot()
{
	bool NewState = !ShipPawn->GetStateManager()->IsPilotMode();
	FLOGV("AFlarePlayerController::TooglePilot : new state is %d", NewState);
	ShipPawn->GetStateManager()->EnablePilot(NewState);
}

void AFlarePlayerController::ToggleHUD()
{
	if (!IsInMenu())
	{
		FLOG("AFlarePlayerController::ToggleHUD");
		GetNavHUD()->ToggleHUD();
	}
	else
	{
		FLOG("AFlarePlayerController::ToggleHUD : don't do it in menus");
	}
}

void AFlarePlayerController::QuickSwitch()
{
	FLOG("AFlarePlayerController::QuickSwitch");

	if (GetGame()->GetActiveSector() && Company)
	{
		TArray<AFlareSpacecraft*> CompanyShips = GetGame()->GetActiveSector()->GetCompanyShips(Company);

		if (CompanyShips.Num())
		{
			int32 QuickSwitchOffset = QuickSwitchNextOffset;
			int32 OffsetIndex = 0;
			AFlareSpacecraft* SeletedCandidate = NULL;

			// First loop in military armed alive ships
			for (int32 ShipIndex = 0; ShipIndex < CompanyShips.Num(); ShipIndex++)
			{
				OffsetIndex = (ShipIndex + QuickSwitchOffset) % CompanyShips.Num();
				AFlareSpacecraft* Candidate = CompanyShips[OffsetIndex];

				if (Candidate && Candidate != ShipPawn && Candidate->CanFight())
				{
					SeletedCandidate = Candidate;
					break;
				}
			}

			// If not, loop in all alive ships
			if (!SeletedCandidate)
			{
				for (int32 ShipIndex = 0; ShipIndex < CompanyShips.Num(); ShipIndex++)
				{
					OffsetIndex = (ShipIndex + QuickSwitchOffset) % CompanyShips.Num();
					AFlareSpacecraft* Candidate = CompanyShips[OffsetIndex];
					if (Candidate && Candidate != ShipPawn && Candidate->GetDamageSystem()->IsAlive())
					{
						SeletedCandidate = Candidate;
						break;
					}
				}
			}

			// Switch to the found ship
			if (SeletedCandidate)
			{
				FLOG("AFlarePlayerController::QuickSwitch : found new ship");
				QuickSwitchNextOffset = OffsetIndex + 1;
				// Disable pilot during the switch
				SeletedCandidate->GetStateManager()->EnablePilot(false);
				MenuManager->OpenMenuSpacecraft(EFlareMenu::MENU_FlyShip, SeletedCandidate);
			}
			else
			{
				FLOG("AFlarePlayerController::QuickSwitch : no ship found");
			}
		}
		else
		{
			FLOG("AFlarePlayerController::QuickSwitch : no ships in company !");
		}
	}
	else
	{
		FLOG("AFlarePlayerController::QuickSwitch : company !");
	}
}

void AFlarePlayerController::MouseInputX(float Val)
{
	if (MenuManager->IsMenuOpen())
	{
		if (MenuPawn)
		{
			MenuPawn->YawInput(Val);
		}
		return;
	}

	if (GetNavHUD()->IsWheelMenuOpen())
	{
		GetNavHUD()->SetWheelCursorMove(FVector2D(Val, 0));
	}
	else if (ShipPawn)
	{
		ShipPawn->YawInput(Val);
	}
}

void AFlarePlayerController::MouseInputY(float Val)
{
	if (MenuManager->IsMenuOpen())
	{
		if (MenuPawn)
		{
			MenuPawn->PitchInput(Val);
		}
		return;
	}

	if (GetNavHUD()->IsWheelMenuOpen())
	{
		GetNavHUD()->SetWheelCursorMove(FVector2D(0, -Val));
	}
	else if (ShipPawn)
	{
		ShipPawn->PitchInput(Val);
	}
}

void AFlarePlayerController::Test1()
{
	IsTest1 = !IsTest1;
	FLOGV("AFlarePlayerController::Test1 %d", IsTest1);
}

void AFlarePlayerController::Test2()
{
	IsTest2 = !IsTest2;
	FLOGV("AFlarePlayerController::Test2 %d", IsTest2);
}


/*----------------------------------------------------
	Wheel menu
----------------------------------------------------*/

void AFlarePlayerController::WheelPressed()
{
	if (GetGame()->IsLoadedOrCreated() && MenuManager && !MenuManager->IsMenuOpen() && !GetNavHUD()->IsWheelMenuOpen())
	{
		TSharedPtr<SFlareMouseMenu> MouseMenu = GetNavHUD()->GetMouseMenu();

		// Setup mouse menu
		MouseMenu->ClearWidgets();
		MouseMenu->AddDefaultWidget("Mouse_Nothing", LOCTEXT("Cancel", "Cancel"));

		// Docked controls
		if (ShipPawn->GetNavigationSystem()->IsDocked())
		{
			if (ShipPawn->GetCurrentSectorInterface()->CanUpgrade(ShipPawn->GetCompany()))
			{
				MouseMenu->AddWidget("ShipUpgrade_Button", LOCTEXT("Upgrade", "Upgrade"),
				FFlareMouseMenuClicked::CreateUObject(this, &AFlarePlayerController::UpgradeShip));
			}
			MouseMenu->AddWidget("Undock_Button", LOCTEXT("Undock", "Undock"),
				FFlareMouseMenuClicked::CreateUObject(this, &AFlarePlayerController::UndockShip));

			// Trade if possible
			if (ShipPawn->GetDescription()->CargoBayCount > 0)
			{
				MouseMenu->AddWidget("Trade_Button", LOCTEXT("Trade", "Trade"),
					FFlareMouseMenuClicked::CreateUObject(this, &AFlarePlayerController::StartTrading));
			}
		}

		// Flying controls
		else
		{
			// Targetting
			AFlareSpacecraft* Target = ShipPawn->GetCurrentTarget();
			if (Target)
			{
				// Inspect
				FText Text = FText::Format(LOCTEXT("InspectTargetFormat", "Inspect {0}"), FText::FromName(Target->GetImmatriculation()));
				MouseMenu->AddWidget(Target->IsStation() ? "Mouse_Inspect_Station" : "Mouse_Inspect_Ship", Text,
					FFlareMouseMenuClicked::CreateUObject(this, &AFlarePlayerController::InspectTargetSpacecraft));

				// Fly
				if (Target->GetCompany() == GetCompany() && !Target->IsStation())
				{
					Text = FText::Format(LOCTEXT("FlyTargetFormat", "Fly {0}"), FText::FromName(Target->GetImmatriculation()));
					MouseMenu->AddWidget("Mouse_Fly", Text,	FFlareMouseMenuClicked::CreateUObject(this, &AFlarePlayerController::FlyTargetSpacecraft));
				}

				// Dock
				if (Target->GetDockingSystem()->HasCompatibleDock(GetShipPawn()) && Target->GetCompany()->GetPlayerWarState() >= EFlareHostility::Neutral)
				{
					Text = FText::Format(LOCTEXT("DockAtTargetFormat", "Dock at {0}"), FText::FromName(Target->GetImmatriculation()));
					MouseMenu->AddWidget("Mouse_DockAt", Text, FFlareMouseMenuClicked::CreateUObject(this, &AFlarePlayerController::DockAtTargetSpacecraft));
				}
			}

			// Capital ship controls
			if (ShipPawn->GetDescription()->Size == EFlarePartSize::L)
			{
				MouseMenu->AddWidget("Mouse_ProtectMe", UFlareGameTypes::GetCombatTacticDescription(EFlareCombatTactic::ProtectMe),
					FFlareMouseMenuClicked::CreateUObject(this, &AFlarePlayerController::SetTacticForCurrentGroup, EFlareCombatTactic::ProtectMe));
				MouseMenu->AddWidget("Mouse_AttackAll", UFlareGameTypes::GetCombatTacticDescription(EFlareCombatTactic::AttackMilitary),
					FFlareMouseMenuClicked::CreateUObject(this, &AFlarePlayerController::SetTacticForCurrentGroup, EFlareCombatTactic::AttackMilitary));
				MouseMenu->AddWidget("Mouse_AttackStations", UFlareGameTypes::GetCombatTacticDescription(EFlareCombatTactic::AttackStations),
					FFlareMouseMenuClicked::CreateUObject(this, &AFlarePlayerController::SetTacticForCurrentGroup, EFlareCombatTactic::AttackStations));
				MouseMenu->AddWidget("Mouse_AttackCivilians", UFlareGameTypes::GetCombatTacticDescription(EFlareCombatTactic::AttackCivilians),
					FFlareMouseMenuClicked::CreateUObject(this, &AFlarePlayerController::SetTacticForCurrentGroup, EFlareCombatTactic::AttackCivilians));
				MouseMenu->AddWidget("Mouse_Nothing", UFlareGameTypes::GetCombatTacticDescription(EFlareCombatTactic::StandDown),
					FFlareMouseMenuClicked::CreateUObject(this, &AFlarePlayerController::SetTacticForCurrentGroup, EFlareCombatTactic::StandDown));
			}

			// Fighter controls
			else
			{
			}
		}

		GetNavHUD()->SetWheelMenu(true);
	}
}

void AFlarePlayerController::WheelReleased()
{
	if (GetGame()->IsLoadedOrCreated())
	{
		GetNavHUD()->SetWheelMenu(false);
	}
}

void AFlarePlayerController::AlignToSpeed()
{
	if (ShipPawn)
	{
		ShipPawn->ForceManual();
		ShipPawn->FaceForward();
	}
}

void AFlarePlayerController::AlignToReverse()
{
	if (ShipPawn)
	{
		ShipPawn->ForceManual();
		ShipPawn->FaceBackward();
	}
}

void AFlarePlayerController::Brake()
{
	if (ShipPawn)
	{
		ShipPawn->ForceManual();
		ShipPawn->Brake();
	}
}

void AFlarePlayerController::InspectTargetSpacecraft()
{
	if (ShipPawn)
	{
		AFlareSpacecraft* TargetSpacecraft = ShipPawn->GetCurrentTarget();
		if (TargetSpacecraft)
		{
			MenuManager->OpenMenuSpacecraft(EFlareMenu::MENU_Ship);
		}
	}
}

void AFlarePlayerController::FlyTargetSpacecraft()
{
	if (ShipPawn)
	{
		AFlareSpacecraft* TargetSpacecraft = ShipPawn->GetCurrentTarget();
		if (TargetSpacecraft)
		{
			// Disable pilot during the switch
			TargetSpacecraft->GetStateManager()->EnablePilot(false);
			MenuManager->OpenMenuSpacecraft(EFlareMenu::MENU_FlyShip, TargetSpacecraft);
		}
	}
}

void AFlarePlayerController::DockAtTargetSpacecraft()
{
	if (ShipPawn)
	{
		AFlareSpacecraft* TargetSpacecraft = ShipPawn->GetCurrentTarget();
		if (TargetSpacecraft)
		{
			bool DockingConfirmed = ShipPawn->GetNavigationSystem()->DockAt(TargetSpacecraft);
			NotifyDockingResult(DockingConfirmed, TargetSpacecraft);
		}
	}
}

void AFlarePlayerController::SetTacticForCurrentGroup(EFlareCombatTactic::Type Tactic)
{
	GetCompany()->GetAI()->SetTacticForCurrentShipGroup(Tactic);
}

void AFlarePlayerController::MatchSpeedWithTargetSpacecraft()
{
	if (ShipPawn)
	{
		AFlareSpacecraft* TargetSpacecraft = ShipPawn->GetCurrentTarget();
		if (TargetSpacecraft)
		{
			ShipPawn->ForceManual();
			ShipPawn->BrakeToVelocity(TargetSpacecraft->GetLinearVelocity());
		}
	}
}

void AFlarePlayerController::LookAtTargetSpacecraft()
{
	if (ShipPawn)
	{
		AFlareSpacecraft* TargetSpacecraft = ShipPawn->GetCurrentTarget();
		if (TargetSpacecraft)
		{
			FVector TargetDirection = (TargetSpacecraft->GetActorLocation() - ShipPawn->GetActorLocation());
			ShipPawn->ForceManual();
			ShipPawn->GetNavigationSystem()->PushCommandRotation(TargetDirection, FVector(1, 0, 0));
		}
	}
}

void AFlarePlayerController::UpgradeShip()
{
	MenuManager->OpenMenuSpacecraft(EFlareMenu::MENU_ShipConfig, ShipPawn);
}

void AFlarePlayerController::UndockShip()
{
	if (ShipPawn)
	{
		ShipPawn->GetNavigationSystem()->Undock();
	}
}

void AFlarePlayerController::StartTrading()
{
	MenuManager->OpenMenuSpacecraft(EFlareMenu::MENU_Trade, ShipPawn);
}


/*----------------------------------------------------
	Config
----------------------------------------------------*/

void AFlarePlayerController::SetUseDarkThemeForStrategy(bool New)
{
	UseDarkThemeForStrategy = New;
	UpdateMenuTheme();
}

void AFlarePlayerController::SetUseDarkThemeForNavigation(bool New)
{
	UseDarkThemeForNavigation = New;
	UpdateMenuTheme();
}

void AFlarePlayerController::SetUseCockpit(bool New)
{
	UseCockpit = New;
	CockpitManager->SetupCockpit(this);
}

void AFlarePlayerController::SetMusicVolume(int32 New)
{
	MusicVolume = New;
	SoundManager->SetMusicVolume(New);
}

void AFlarePlayerController::SetMasterVolume(int32 New)
{
	MasterVolume = New;
	SoundManager->SetMasterVolume(New);
}


/*----------------------------------------------------
	Getters for game classes
----------------------------------------------------*/

UFlareSimulatedSpacecraft* AFlarePlayerController::GetLastFlownShip()
{
	UFlareWorld* GameWorld = GetGame()->GetGameWorld();
	if (GameWorld)
	{
		return GameWorld->FindSpacecraft(PlayerData.LastFlownShipIdentifier);
	}
	else
	{
		return NULL;
	}
}


#undef LOCTEXT_NAMESPACE
