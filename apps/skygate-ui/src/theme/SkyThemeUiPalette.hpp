#pragma once

#include <QColor>

namespace skygate::ui::internal {

#define SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(X)                                                                        \
    X(windowBackground)                                                                                                \
    X(cardBackground)                                                                                                  \
    X(cardBackgroundBottom)                                                                                            \
    X(cardBorder)                                                                                                      \
    X(dividerColor)                                                                                                    \
    X(headerGradientStart)                                                                                             \
    X(headerGradientEnd)                                                                                               \
    X(textPrimary)                                                                                                     \
    X(textSecondary)                                                                                                   \
    X(textMuted)                                                                                                       \
    X(formLabelText)                                                                                                   \
    X(sectionTitleText)                                                                                                \
    X(monospaceText)                                                                                                   \
    X(toolbarPanelBackground)                                                                                          \
    X(toolbarPanelBorder)                                                                                              \
    X(toolbarButtonText)                                                                                               \
    X(toolbarButtonBackground)                                                                                         \
    X(toolbarButtonBackgroundHover)                                                                                    \
    X(toolbarButtonBackgroundPressed)                                                                                  \
    X(toolbarButtonBorder)                                                                                             \
    X(toolbarToggleText)                                                                                               \
    X(toolbarToggleBackground)                                                                                         \
    X(toolbarToggleBackgroundHover)                                                                                    \
    X(toolbarToggleBackgroundPressed)                                                                                  \
    X(toolbarToggleBorder)                                                                                             \
    X(toolbarDropdownBackground)                                                                                       \
    X(toolbarDropdownBorder)                                                                                           \
    X(toolbarItemBackgroundHover)                                                                                      \
    X(toolbarItemBackgroundSelected)                                                                                   \
    X(toolbarPrimaryText)                                                                                              \
    X(toolbarSecondaryText)                                                                                            \
    X(inputText)                                                                                                       \
    X(inputPlaceholderText)                                                                                            \
    X(inputBackground)                                                                                                 \
    X(inputBackgroundDisabled)                                                                                         \
    X(inputBorder)                                                                                                     \
    X(inputBorderFocus)                                                                                                \
    X(inputSelection)                                                                                                  \
    X(inputSelectionText)                                                                                              \
    X(inputIndicator)                                                                                                  \
    X(listItemPrimaryText)                                                                                             \
    X(listItemSecondaryText)                                                                                           \
    X(listItemSectionText)                                                                                             \
    X(listItemBackgroundHover)                                                                                         \
    X(listItemBackgroundSelected)                                                                                      \
    X(emptyStateText)                                                                                                  \
    X(actionButtonPrimaryTop)                                                                                          \
    X(actionButtonPrimaryTopHover)                                                                                     \
    X(actionButtonPrimaryTopPressed)                                                                                   \
    X(actionButtonPrimaryBottom)                                                                                       \
    X(actionButtonPrimaryBottomHover)                                                                                  \
    X(actionButtonPrimaryBottomPressed)                                                                                \
    X(actionButtonPrimaryBorder)                                                                                       \
    X(actionButtonSecondaryTop)                                                                                        \
    X(actionButtonSecondaryTopHover)                                                                                   \
    X(actionButtonSecondaryTopPressed)                                                                                 \
    X(actionButtonSecondaryBottom)                                                                                     \
    X(actionButtonSecondaryBottomHover)                                                                                \
    X(actionButtonSecondaryBottomPressed)                                                                              \
    X(actionButtonSecondaryBorder)                                                                                     \
    X(actionButtonDisabledTop)                                                                                         \
    X(actionButtonDisabledBottom)                                                                                      \
    X(actionButtonDisabledBorder)                                                                                      \
    X(actionButtonText)                                                                                                \
    X(actionButtonTextDisabled)                                                                                        \
    X(sectionButtonActiveTop)                                                                                          \
    X(sectionButtonActiveTopHover)                                                                                     \
    X(sectionButtonActiveTopPressed)                                                                                   \
    X(sectionButtonActiveBottom)                                                                                       \
    X(sectionButtonActiveBottomHover)                                                                                  \
    X(sectionButtonActiveBottomPressed)                                                                                \
    X(sectionButtonActiveBorder)                                                                                       \
    X(sectionButtonInactiveTop)                                                                                        \
    X(sectionButtonInactiveTopHover)                                                                                   \
    X(sectionButtonInactiveTopPressed)                                                                                 \
    X(sectionButtonInactiveBottom)                                                                                     \
    X(sectionButtonInactiveBottomHover)                                                                                \
    X(sectionButtonInactiveBottomPressed)                                                                              \
    X(sectionButtonInactiveBorder)                                                                                     \
    X(sectionButtonTextActive)                                                                                         \
    X(sectionButtonTextInactive)                                                                                       \
    X(closeButtonText)                                                                                                 \
    X(closeButtonBackground)                                                                                           \
    X(closeButtonBackgroundHover)                                                                                      \
    X(closeButtonBackgroundPressed)                                                                                    \
    X(closeButtonBorder)                                                                                               \
    X(quoteBackground)                                                                                                 \
    X(quoteBorder)                                                                                                     \
    X(quoteText)                                                                                                       \
    X(progressBarTrack)                                                                                                \
    X(progressBarTrackBorder)                                                                                          \
    X(progressBarSweepStart)                                                                                           \
    X(progressBarSweepMid)                                                                                             \
    X(progressBarSweepEnd)                                                                                             \
    X(footerBackground)                                                                                                \
    X(footerLiveText)                                                                                                  \
    X(footerPausedText)                                                                                                \
    X(footerLocationText)                                                                                              \
    X(footerInfoText)                                                                                                  \
    X(footerTimeText)                                                                                                  \
    X(footerTimeHoverBackground)                                                                                       \
    X(footerTimeHoverBorder)                                                                                           \
    X(errorText)                                                                                                       \
    X(checkBackground)                                                                                                 \
    X(checkBorder)                                                                                                     \
    X(checkBorderChecked)                                                                                              \
    X(checkMarkColor)

struct SkyThemeUiPalette final {
#define SKYGATE_DECLARE_UI_COLOR_MEMBER(name) QColor name;
    SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(SKYGATE_DECLARE_UI_COLOR_MEMBER)
#undef SKYGATE_DECLARE_UI_COLOR_MEMBER
};

}  // namespace skygate::ui::internal
