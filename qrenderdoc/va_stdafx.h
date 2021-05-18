// This macro confuses Visual Assist a lot which can lead to a lot of different problems,
// including refactoring adding QT_NAMESPACE onto classes like QString, and Ui classes
// not being found properly.

#define QT_NAMESPACE

// This is used to conditionally stop visual assist from parsing swig-generated files

#define VA_IGNORE_REST_OF_FILE _asm {
