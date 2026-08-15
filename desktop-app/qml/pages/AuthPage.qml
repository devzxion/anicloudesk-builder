import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AniCloud

Item {
    id: root
    property string mode: "login"
    property string flowEmail: ""

    Rectangle {
        width: Math.min(460, parent.width - 50); height: form.implicitHeight + 58
        anchors.centerIn: parent; radius: 16; color: Theme.surface; border.color: Theme.border
        ColumnLayout {
            id: form; anchors.left: parent.left; anchors.right: parent.right; anchors.centerIn: parent; anchors.margins: 28; spacing: 13
            Text { text: root.mode === "login" ? "Welcome back" : root.mode === "register" ? "Create your account" : root.mode === "verify" ? "Verify your email" : root.mode === "forgot" ? "Reset password" : "Choose a new password"; color: Theme.text; font.pixelSize: 26; font.weight: Font.Black; Layout.fillWidth: true }
            Text { visible: Account.error.length > 0; text: Account.error; color: Theme.red; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            TextField { id: nameField; visible: root.mode === "register"; placeholderText: "Name"; color: Theme.text; Layout.fillWidth: true; Accessible.name: "Name"; background: Rectangle { color: Theme.raised; radius: Theme.radius; border.color: nameField.activeFocus ? Theme.red : Theme.border } }
            TextField { id: emailField; visible: ["login", "register", "forgot"].indexOf(root.mode) >= 0; placeholderText: "Email"; color: Theme.text; Layout.fillWidth: true; inputMethodHints: Qt.ImhEmailCharactersOnly; Accessible.name: "Email"; background: Rectangle { color: Theme.raised; radius: Theme.radius; border.color: emailField.activeFocus ? Theme.red : Theme.border } }
            TextField { id: otpField; visible: root.mode === "verify" || root.mode === "reset"; placeholderText: "6-digit code"; color: Theme.text; Layout.fillWidth: true; inputMethodHints: Qt.ImhDigitsOnly; maximumLength: 8; Accessible.name: "Verification code"; background: Rectangle { color: Theme.raised; radius: Theme.radius; border.color: otpField.activeFocus ? Theme.red : Theme.border } }
            TextField {
                id: passwordField
                visible: root.mode === "login" || root.mode === "register" || root.mode === "reset"
                placeholderText: root.mode === "reset" ? "New password" : "Password"
                color: Theme.text; Layout.fillWidth: true; echoMode: TextInput.Password
                Accessible.name: placeholderText
                background: Rectangle { color: Theme.raised; radius: Theme.radius; border.color: passwordField.activeFocus ? Theme.red : Theme.border }
                onAccepted: root.submit()
            }
            AppButton { Layout.fillWidth: true; text: Account.busy ? "Please wait…" : root.mode === "login" ? "Sign in" : root.mode === "register" ? "Register" : root.mode === "verify" ? "Verify" : root.mode === "forgot" ? "Send reset code" : "Reset password"; enabled: !Account.busy; onClicked: root.submit() }
            RowLayout {
                Layout.fillWidth: true
                Button { visible: root.mode === "login"; text: "Create account"; flat: true; palette.buttonText: Theme.muted; onClicked: root.mode = "register" }
                Button { visible: root.mode === "login"; text: "Forgot password?"; flat: true; palette.buttonText: Theme.muted; onClicked: root.mode = "forgot" }
                Button { visible: root.mode !== "login"; text: "Back to sign in"; flat: true; palette.buttonText: Theme.muted; onClicked: root.mode = "login" }
                Item { Layout.fillWidth: true }
                Button { visible: root.mode === "verify"; text: "Resend code"; flat: true; palette.buttonText: Theme.red; onClicked: Account.resendVerification(root.flowEmail) }
            }
        }
    }
    Connections {
        target: Account
        function onVerificationRequired(email, expiresInMinutes) { root.flowEmail = email; root.mode = "verify" }
        function onPasswordResetRequested(email) { root.flowEmail = email; root.mode = "reset" }
        function onAuthenticationChanged() { if (Account.authenticated) { Runtime.restorePendingRoute(); if (Runtime.route === "auth") Runtime.route = "home" } }
    }
    function submit() {
        if (mode === "login") Account.login(emailField.text.trim(), passwordField.text)
        else if (mode === "register") Account.registerAccount(nameField.text.trim(), emailField.text.trim(), passwordField.text)
        else if (mode === "verify") Account.verifyEmail(flowEmail, otpField.text.trim())
        else if (mode === "forgot") Account.forgotPassword(emailField.text.trim())
        else Account.resetPassword(flowEmail, otpField.text.trim(), passwordField.text)
    }
}
