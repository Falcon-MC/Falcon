#pragma once

// script-side parts of the API; each source evaluates to a function: natives go in, official classes come out,
// and nothing reaches the global object
namespace ScriptBootstraps {
    // (names) => { classes, make(name, message, fields) }
    const char *errors();

    // (native) => { BlockPermutation, BlockType, BlockTypes, BlockStates, ItemType, ItemTypes, installJobs }
    const char *data();

    // (native) => { BlockVolumeBase, BlockVolume, ListBlockVolume, BlockLocationIterator }
    const char *volumes();

    // (native) => { ActionFormData, MessageFormData, ModalFormData }
    const char *ui();
}
