#pragma once

/**
 * The format differences of a protocol version that are shared by several packets or too small to justify a
 * serializer of their own. Packet code reads them from its codec context; the defaults describe the current
 * protocol.
 */
struct ProtocolCapabilities {
    int mProtocolVersion = 0;

    /**
     * Older versions put an extra "true" before several optional fields, so they read as present twice.
     */
    bool mDoubledPresence = false;

    bool mBossEventPlayerId = false;
    bool mItemUseHand = true;
    bool mCameraStartingRotation = true;
    bool mAttributeNoiseAlignment = true;
    bool mMoveDeltaTicks = true;
    bool mPlaySoundRangeAndPosition = true;
    bool mSubChunkHeightMapRows = true;
    bool mDiagnosticsActorPosition = true;

    bool mAttributePayloads = true;
    bool mPassengerOfBlock = true;
    bool mAnimateHand = true;
    bool mDimensionClouds = true;
    bool mTransactionHandOnEveryUse = true;
    bool mCraftReservedAction = true;
    bool mLevelChunkBiomeUpdate = true;

    /**
     * The PlayFab ID moved from the skin to the player list entry.
     */
    bool mPlayerListPlayFabId = true;

    bool mStartGameEditorMigration = true;
    bool mEducationAgentCapabilities = true;
    bool mSoundDataUpdateType = true;
    bool mDiagnosticsOptionalFields = true;
};
