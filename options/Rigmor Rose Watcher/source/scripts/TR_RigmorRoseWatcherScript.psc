Scriptname TR_RigmorRoseWatcherScript Extends Quest

; Rigmor of Bruma's own QF_RigmorQuest00 script calls Rose.EvaluatePackage() / Alias_Hedren's
; EvaluatePackage() after changing an actor's AI package on some stages but not others, so the
; new package/dialogue availability doesn't visibly take effect until something else forces a
; re-evaluation -- which is why leaving/re-entering a cell "unsticks" a stalled scene.
; This periodically forces that re-evaluation ourselves instead of touching Rigmor's own script.
; Hedren added 2026-08-26 after stage 65 (RigmorQuest00scene165) stalled the same way Rose did.
;
; RigmorQuest00scene65 fix added 2026-08-26: Scene65 ("she's lost a lot of blood") is supposed to
; end by calling GetOwningQuest().SetStage(70) in its own SF_RigmorQuest00scene65 fragment, and
; stage 70 has NO code of its own in the main quest script (no Fragment_70) to force a refresh --
; so if that SetStage(70) call itself gets lost/delayed, the "What do you need?" topic never
; becomes selectable.
;
; Revised same day after confirmed-via-trace-log that the scene doesn't finish and hand off --
; it gets stuck genuinely PLAYING (Scene.IsPlaying() stays true indefinitely, matching the user's
; report of Rose repeatedly starting to speak and stopping). So a "wait for it to finish" safety
; net can never fire. Instead: once the scene has been stuck playing for a sustained period, force
; it to stop and push the stage forward ourselves. Stopping an already-desynced scene just clears
; whatever internal state had it wedged; the stage-70 push is safe because stage 70's own
; Fragment is empty. Threshold tuned to 15s (Rigmor's scenes here run just a few short lines).

Quest Property RigmorQuest00 Auto
Actor Property Rose Auto
Actor Property Hedren Auto
Scene Property RigmorQuest00scene65 Auto

Int stuckTicks = 0

Event OnInit()
    Debug.Trace("TR_RigmorRoseWatcher: OnInit fired, RigmorQuest00=" + RigmorQuest00 + " Rose=" + Rose + " Hedren=" + Hedren + " Scene65=" + RigmorQuest00scene65)
    RegisterForSingleUpdate(3.0)
EndEvent

Event OnUpdate()
    ; Always re-register: there's no reliable stage/quest-state to detect "permanently done"
    ; (IsRunning() is also false before the quest has even started), and the cost of an idle
    ; check every 3 seconds for the rest of the playthrough is negligible either way.
    If !RigmorQuest00.IsRunning()
        RegisterForSingleUpdate(3.0)
    Else
        Int stage = RigmorQuest00.GetStage()
        Bool scene65Playing = RigmorQuest00scene65.IsPlaying()
        Debug.Trace("TR_RigmorRoseWatcher: tick, stage=" + stage + " Rose.IsInDialogueWithPlayer=" + Rose.IsInDialogueWithPlayer() + " Scene65.IsPlaying=" + scene65Playing + " stuckTicks=" + stuckTicks)

        If Rose && Rose.Is3DLoaded() && !Rose.IsInDialogueWithPlayer()
            Rose.EvaluatePackage()
        EndIf
        If Hedren && Hedren.Is3DLoaded() && !Hedren.IsInDialogueWithPlayer()
            Hedren.EvaluatePackage()
        EndIf

        If stage >= 65 && stage < 70
            If scene65Playing
                stuckTicks += 1
                If stuckTicks >= 5 ; ~15s at 3s/tick
                    Debug.Trace("TR_RigmorRoseWatcher: Scene65 stuck playing for " + stuckTicks + " ticks at stage " + stage + " -- forcing Stop() and SetStage(70)")
                    RigmorQuest00scene65.Stop()
                    RigmorQuest00.SetStage(70)
                    stuckTicks = 0
                EndIf
            Else
                Debug.Trace("TR_RigmorRoseWatcher: Scene65 finished but stage stuck at " + stage + " -- forcing SetStage(70)")
                RigmorQuest00.SetStage(70)
                stuckTicks = 0
            EndIf
        Else
            stuckTicks = 0
        EndIf

        RegisterForSingleUpdate(3.0)
    EndIf
EndEvent
