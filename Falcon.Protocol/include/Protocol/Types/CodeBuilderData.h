#pragma once

enum class CodeBuilderOperationType {
    None,
    Get,
    Set,
    Reset,
};

enum class CodeBuilderCategoryType {
    None,
    CodeStatus,
    Instantiation,
};

enum class CodeBuilderCodeStatus {
    None,
    NotStarted,
    InProgress,
    Paused,
    Error,
    Succeeded,
};
