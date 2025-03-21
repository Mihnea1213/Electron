#include <SFML/Graphics.hpp>
#include <SFML/Window/Clipboard.hpp>
#include <iostream>
#include <queue>
#include <string>
#include <fstream>
#include <sstream>
#include <memory>
#include <set>
#include <map>
#include <algorithm>

//                  VARIABILE SI FUNCTII MENIU-------------------------------------------------------------------------------------------
// **Modified: Changed from const to variable to allow dynamic assignment**
int SCREEN_WIDTH;
int SCREEN_HEIGHT;

const int MAX_BUTTONS = 5;
sf::Clock pageTransitionClock;
const float PAGE_TRANSITION_COOLDOWN = 0.3f;
int idAct;

enum class ApplicationPage {
    MainMenu,
    Settings,
    Information,
    NewComponent,
    CancelConfirmation
};

struct Button {
    sf::RectangleShape shape;
    sf::Text text;
    bool isHovered;
};

struct TextInputBox {
    sf::RectangleShape shape;
    sf::Text displayText;
    std::string inputText;
    bool isActive;
    sf::RectangleShape cursorShape;
    float cursorBlinkTimer;

    // Scrollbar data
    sf::RectangleShape scrollBarTrack;
    sf::RectangleShape scrollBarThumb;
    float scrollOffset;
    float contentHeight;
    bool isDraggingThumb;
    float thumbClickOffset;

    // Selection data
    int selectionStart;
    int selectionEnd;
    bool isSelecting;
    sf::RectangleShape selectionHighlight;

    TextInputBox()
        : inputText(""), isActive(false), cursorBlinkTimer(0.0f),
        scrollOffset(0.0f), contentHeight(0.0f),
        isDraggingThumb(false), thumbClickOffset(0.0f),
        selectionStart(0), selectionEnd(0), isSelecting(false)
    {
        // Default highlight style
        selectionHighlight.setFillColor(sf::Color(100, 100, 255, 100));
    }
};

class PieceDrawer {
public:
    struct Link {
        float x, y;
    };

    struct Command {
        std::string type;
        float x1, y1, x2, y2;
    };

    struct Piece {
        std::string name;
        std::vector<Link> links;
        std::vector<Command> commands;
    };
    Piece piece;
    std::string fileNameForDrawer;

    PieceDrawer(const std::string& filename) {
        fileNameForDrawer = filename;
        if (!loadPieceFromFile(filename)) {
            std::cerr << "Failed to load piece data!" << std::endl;
        }
    }

    bool loadPieceFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file!" << std::endl;
            return false;
        }

        std::string line;
        if (!std::getline(file, piece.name)) {
            return false;
        }

        int numLinks;
        file >> numLinks;
        file.ignore();
        if (numLinks < 0)
        {
            return false;
        }
        piece.links.resize(numLinks);
        for (auto& link : piece.links) {
            file >> link.x >> link.y;
        }

        while (std::getline(file, line) && line.empty()) {}

        int numCommands;
        file >> numCommands;
        file.ignore();
        if (numCommands < 0)
        {
            return false;
        }
        piece.commands.resize(numCommands);
        for (auto& command : piece.commands) {
            file >> command.type >> command.x1 >> command.y1 >> command.x2 >> command.y2;
        }

        return true;
    }

    // Adaugă această funcție auxiliară pentru a roti punctele
    sf::Vector2f rotatePoint(const sf::Vector2f& point, const sf::Vector2f& origin, float angleDegrees) {
        float angleRad = angleDegrees * 3.14159265f / 180.f;
        float s = sin(angleRad);
        float c = cos(angleRad);

        // Translate point back to origin:
        float xnew = point.x - origin.x;
        float ynew = point.y - origin.y;

        // Rotate point
        float xrot = xnew * c - ynew * s;
        float yrot = xnew * s + ynew * c;

        // Translate point back:
        xrot += origin.x;
        yrot += origin.y;

        return sf::Vector2f(xrot, yrot);
    }

    void drawPiece(sf::RenderWindow& window, sf::Vector2f position, float scale, float rotationAngle = 0.f) {
        // Poziția de origine pentru rotație (centrul piesei)
        sf::Vector2f origin = position;

        for (const auto& link : piece.links) {
            sf::CircleShape dot(5);
            dot.setFillColor(sf::Color::Red);
            sf::Vector2f originalPos = sf::Vector2f(link.x, link.y) * scale + position;
            sf::Vector2f rotatedPos = rotatePoint(originalPos, origin, rotationAngle);
            dot.setPosition(rotatedPos.x, rotatedPos.y);
            window.draw(dot);
        }

        for (const auto& command : piece.commands) {
            if (command.type == "L") {
                sf::Vertex line[2];
                line[0].color = sf::Color::White;
                line[1].color = sf::Color::White;

                sf::Vector2f p1 = sf::Vector2f(command.x1, command.y1) * scale + position;
                sf::Vector2f p2 = sf::Vector2f(command.x2, command.y2) * scale + position;

                p1 = rotatePoint(p1, origin, rotationAngle);
                p2 = rotatePoint(p2, origin, rotationAngle);

                line[0].position = p1;
                line[1].position = p2;
                window.draw(line, 2, sf::Lines);
            }
            else if (command.type == "R") {
                sf::RectangleShape rect(sf::Vector2f(std::abs(command.x2 - command.x1) * scale, std::abs(command.y2 - command.y1) * scale));
                rect.setPosition(command.x1 * scale + position.x, command.y1 * scale + position.y);
                rect.setFillColor(sf::Color::Blue);
                rect.setRotation(rotationAngle);
                window.draw(rect);
            }
            else if (command.type == "O") {
                float radius = std::abs(command.x2 - command.x1) * scale;
                sf::CircleShape circle(radius);
                circle.setFillColor(sf::Color::Green);
                sf::Vector2f center = sf::Vector2f(command.x1, command.y1) * scale + position;
                sf::Vector2f rotatedCenter = rotatePoint(center, origin, rotationAngle);
                circle.setPosition(rotatedCenter.x - radius, rotatedCenter.y - radius);
                window.draw(circle);
            }
        }
    }
    const std::vector<Link>& getLinks() const {
        return piece.links;
    }

private:
};

PieceDrawer* refreshPiece;

// Forward Declarations
void initializeWindow(sf::RenderWindow& window);
sf::Font loadSansSerifFont();
void handleGlobalEvents(const sf::Event& event, bool& running);
void drawTitleScreen(sf::RenderWindow& window, sf::Font& font);
void drawMainMenu(sf::RenderWindow& window, sf::Font& font, Button* buttons, int buttonCount);
void drawSettingsMenu(sf::RenderWindow& window, sf::Font& font, Button* buttons, int buttonCount);
void drawInformationPage(sf::RenderWindow& window, sf::Font& font, const sf::Text& infoText, Button* buttons, int buttonCount);
void updateButtonColors(Button* buttons, int buttonCount, sf::RenderWindow& window);
void createMainMenuButtons(Button* buttons, int& buttonCount, sf::Font& font);
void createSettingsButtons(Button* buttons, int& buttonCount, sf::Font& font);
void createInformationButtons(Button* buttons, int& buttonCount, sf::Font& font);
void handleMainMenuInteractions(const sf::Event& event, sf::RenderWindow& window, Button* buttons, int buttonCount, bool& running, ApplicationPage& currentPage);
void handleSettingsInteractions(const sf::Event& event, sf::RenderWindow& window, Button* buttons, int buttonCount, ApplicationPage& currentPage);
void handleInformationInteractions(const sf::Event& event, sf::RenderWindow& window, Button* buttons, int buttonCount, ApplicationPage& currentPage);
void createNewComponentButtons(Button* buttons, int& buttonCount, sf::Font& font);
void drawNewComponentPage(sf::RenderWindow& window, sf::Font& font, Button* buttons, int buttonCount, TextInputBox& inputBox);
void handleNewComponentInteractions(const sf::Event& event, sf::RenderWindow& window, Button* buttons, int buttonCount, ApplicationPage& currentPage, TextInputBox& inputBox);
void createCancelConfirmationButtons(Button* buttons, int& buttonCount, sf::Font& font);
void drawCancelConfirmationPopup(sf::RenderWindow& window, sf::Font& font, Button* buttons, int buttonCount);
void handleCancelConfirmationInteractions(const sf::Event& event, sf::RenderWindow& window, Button* buttons, int buttonCount, ApplicationPage& currentPage);

void handleTextInput(TextInputBox& inputBox, const sf::Event& event);
void updateTextInputBox(TextInputBox& inputBox, sf::Font& font, float deltaTime);
sf::Text loadInformationContent(sf::Font& font, const std::string& filePath);
void handleTextBoxScrolling(TextInputBox& inputBox, const sf::Event& event);
int getCharIndexAtPos(const TextInputBox& inputBox, const sf::Vector2f& relativePos, const sf::Font& font);

TextInputBox componentNameBox;

//   VARIABILE SI FUNCTII WORKSPACE--------------------------------------------------------------------------------------
// Variabile pentru grid
const float distanceThreshold = 100.f;
float gridSpacing = 50.f; // Distanța între linii
sf::Color gridColor(100, 100, 100, 150); // Gri cu transparență
float scale = 20.f;
float squareSize = 10.f; // Dimensiunea pătratului (poți ajusta după necesități)
const float hitboxSize = 80.f; // Dimensiunea hitbox-ului

// Comparator pentru sf::Vector2f
struct Vector2fCompare {
    bool operator()(const sf::Vector2f& a, const sf::Vector2f& b) const {
        if (a.x != b.x)
            return a.x < b.x;
        return a.y < b.y;
    }
};

// Funcție pentru desenarea gridului
void drawGrid(sf::RenderWindow& window, sf::View& view, float spacing, sf::Color color) {
    sf::Vector2f viewCenter = view.getCenter();
    sf::Vector2f viewSize = view.getSize();
    sf::Vector2f viewTopLeft(viewCenter.x - viewSize.x / 2, viewCenter.y - viewSize.y / 2);
    float startX = std::floor(viewTopLeft.x / spacing) * spacing;
    float endX = startX + viewSize.x + spacing;
    float startY = std::floor(viewTopLeft.y / spacing) * spacing;
    float endY = startY + viewSize.y + spacing;

    // Desenăm punctele la intersecția liniilor
    sf::CircleShape point(2.f); // Punct cu raza 2
    point.setFillColor(color);
    for (float x = startX; x <= endX; x += spacing) {
        for (float y = startY; y <= endY; y += spacing) {
            point.setPosition(x - point.getRadius(), y - point.getRadius());
            window.draw(point);
        }
    }
}
const sf::FloatRect sheetBounds(0, 0, 5000, 4000); // Dimensiunea foii metrice



//Clasa pentru componente mobile
class DraggableSquare {
public:
    std::vector<std::string> componentValues;
    int id;
    sf::Vector2f position;
    bool isBeingDragged = false;
    sf::Vector2f dragOffset;
    std::shared_ptr<PieceDrawer> pieceDrawer;

    bool hitboxVisible = false;                  // Flag pentru vizibilitatea hitbox-ului
    sf::RectangleShape hitbox;                   // Dreptunghiul pentru hitbox
    sf::CircleShape rotateCircle;                // Cercul albastru pentru rotație
    bool isRotating = false;                     // Flag pentru starea de rotație
    float initialAngle = 0.f;                    // Unghiul inițial înainte de rotație
    sf::Vector2f rotationStartMousePos;          // Poziția mouse-ului la începutul rotației
    // Pentru detectarea dublu-click-ului
    bool firstClick = false;
    sf::Clock doubleClickClock;
    sf::Time lastClickTime;
    float localScale = 1.f;          // Factor de scalare individual
    float rotationAngle = 0.f;       // Unghiul de rotație
    DraggableSquare(std::shared_ptr<PieceDrawer> drawer, sf::Vector2f pos) : pieceDrawer(drawer), position(pos) {
        // Inițializează hitbox-ul
        hitbox.setFillColor(sf::Color::Transparent);
        hitbox.setOutlineColor(sf::Color::Magenta);
        hitbox.setOutlineThickness(2.f);
        id = idAct;
        idAct++;

        // Inițializează cercul de rotație
        rotateCircle.setRadius(8.f);
        rotateCircle.setFillColor(sf::Color::Blue);
        rotateCircle.setOrigin(rotateCircle.getRadius(), rotateCircle.getRadius()); // Centrare origine
        // Limitează localScale
        if (localScale < 0.5f)
            localScale = 0.5f;
        if (localScale > 2.f)
            localScale = 2.f;
        componentValues.resize(12, "0");
    }

    void startDragging(sf::Vector2f mousePos) {
        isBeingDragged = true;
        dragOffset = mousePos - position;
    }

    void stopDragging() {
        isBeingDragged = false;
    }

    void updateDrag(sf::Vector2f mousePos) {
        if (isBeingDragged) {
            position = mousePos - dragOffset;
        }
    }

    void update(sf::Vector2f newPos) {
        if (isBeingDragged)
            position = newPos;
    }

    void draw(sf::RenderWindow& window) {
        // Desenează piesa la poziția specificată cu scalare și rotație
        pieceDrawer->drawPiece(window, position, scale * localScale, rotationAngle);

        // Actualizează hitbox-ul la poziția curentă, indiferent dacă este vizibil
        // Setează dimensiunea hitbox-ului cu padding
        hitbox.setSize(sf::Vector2f(hitboxSize * localScale, hitboxSize * localScale));

        // Setează originea la centrul hitbox-ului pentru rotație
        hitbox.setOrigin(hitbox.getSize().x / 2.f, hitbox.getSize().y / 2.f);

        // Setează poziția la centrul componentei
        hitbox.setPosition(position.x, position.y);

        // Setează rotația hitbox-ului
        hitbox.setRotation(rotationAngle);

        // Desenează hitbox-ul și cercul de rotație doar dacă este vizibil
        if (hitboxVisible) {
            window.draw(hitbox);

            // Calculăm centrul componentei
            sf::Vector2f center = position;

            // Define offset pentru cercul de rotație relativ la colțul dreapta sus al hitbox-ului
            float extraPadding = 0.f; // Poate fi ajustat dacă este necesar
            sf::Vector2f rotateCircleOffset((hitboxSize * localScale) / 2.f - rotateCircle.getRadius() - extraPadding,
                -(hitboxSize * localScale) / 2.f + rotateCircle.getRadius() + extraPadding);

            // Rotim offset-ul în funcție de unghiul de rotație
            sf::Vector2f rotatedOffset = rotatePoint(rotateCircleOffset, sf::Vector2f(0.f, 0.f), rotationAngle);

            // Setează poziția cercului de rotație în colțul dreapta sus
            rotateCircle.setPosition(center.x + rotatedOffset.x, center.y + rotatedOffset.y);
            rotateCircle.setRotation(rotationAngle); // Opțional: rotește cercul de rotație pentru consistență
            window.draw(rotateCircle);
        }
    }

    // Metodă pentru a obține poziția absolută a unui port dat indexul
    sf::Vector2f rotatePoint(const sf::Vector2f& point, const sf::Vector2f& origin, float angleDegrees) const {
        float angleRad = angleDegrees * 3.14159265f / 180.f;
        float s = sin(angleRad);
        float c = cos(angleRad);

        // Translate point back to origin:
        float xnew = point.x - origin.x;
        float ynew = point.y - origin.y;

        // Rotate point
        float xrot = xnew * c - ynew * s;
        float yrot = xnew * s + ynew * c;

        // Translate point back:
        xrot += origin.x;
        yrot += origin.y;

        return sf::Vector2f(xrot, yrot);
    }

    sf::Vector2f getPortPosition(int portIndex) const {
        if (portIndex < 0 || portIndex >= pieceDrawer->getLinks().size()) {
            return position; // Returnează poziția componentei dacă indexul este invalid
        }
        const auto& link = pieceDrawer->getLinks()[portIndex];
        sf::Vector2f unrotatedPos = sf::Vector2f(link.x * scale * localScale + position.x, link.y * scale * localScale + position.y);

        // Rotim poziția portului în jurul position
        return rotatePoint(unrotatedPos, position, rotationAngle);
    }
};
//Clasa pentru piese statice meniu
class StaticSquare {
public:
    sf::Vector2f position;
    std::shared_ptr<PieceDrawer> pieceDrawer;
    sf::RectangleShape hitbox; // Hitbox pentru componenta statică

    StaticSquare(std::shared_ptr<PieceDrawer> drawer, sf::Vector2f pos)
        : pieceDrawer(drawer), position(pos) {
        // Inițializează hitbox-ul
        hitbox.setSize(sf::Vector2f(hitboxSize, hitboxSize));
        hitbox.setFillColor(sf::Color::Transparent);
        hitbox.setOutlineColor(sf::Color::Magenta);
        hitbox.setOutlineThickness(2.f);
        // Setează originea la centrul hitbox-ului
        hitbox.setOrigin(hitbox.getSize().x / 2.f, hitbox.getSize().y / 2.f);
        // Setează poziția hitbox-ului
        hitbox.setPosition(position.x, position.y);
    }

    bool isMouseOver(const sf::Vector2f& mousePos) const {
        float rectSize = squareSize; // Utilizează dimensiunea globală a pătratului
        return sf::FloatRect(position.x, position.y, rectSize, rectSize).contains(mousePos);
    }

    std::shared_ptr<DraggableSquare> createDraggableSquare(sf::Vector2f pos) {
        return std::make_shared<DraggableSquare>(pieceDrawer, pos);
    }

    void draw(sf::RenderWindow& window) {
        pieceDrawer->drawPiece(window, position, scale, 0.f); // Rotație inițială la 0
    }

    sf::FloatRect getHitbox() const { // Metodă pentru a obține hitbox-ul
        return hitbox.getGlobalBounds();
    }

    sf::Vector2f getPortPosition(int portIndex) const {
        if (portIndex < 0 || portIndex >= pieceDrawer->getLinks().size()) {
            return position; // Returnează poziția componentei dacă indexul este invalid
        }
        const auto& link = pieceDrawer->getLinks()[portIndex];
        return sf::Vector2f(link.x * scale + position.x, link.y * scale + position.y);
    }
};

struct Connection {
    std::shared_ptr<DraggableSquare> sourceComponent;
    int sourcePortIndex;
    std::shared_ptr<DraggableSquare> targetComponent;
    int targetPortIndex;
    bool isInitialDirectionUp; // Nou: true dacă inițial a mers în sus, false dacă a mers în jos

    // Constructor
    Connection(std::shared_ptr<DraggableSquare> srcComp, int srcPort, std::shared_ptr<DraggableSquare> tgtComp, int tgtPort, bool directionUp = true)
        : sourceComponent(srcComp), sourcePortIndex(srcPort),
        targetComponent(tgtComp), targetPortIndex(tgtPort),
        isInitialDirectionUp(directionUp) {
    }
};

// Viteza de mișcare a camerei
const float cameraSpeed = 10.f; // Poți ajusta această valoare după preferințe

// Funcție pentru a verifica dacă o linie (p1, p2) intersectează un dreptunghi (rect)
bool lineIntersectsRect(const sf::Vector2f& p1, const sf::Vector2f& p2, const sf::FloatRect& rect) {
    // Definirea colțurilor dreptunghiului
    sf::Vector2f r1(rect.left, rect.top);
    sf::Vector2f r2(rect.left + rect.width, rect.top);
    sf::Vector2f r3(rect.left + rect.width, rect.top + rect.height);
    sf::Vector2f r4(rect.left, rect.top + rect.height);

    // Funcție auxiliară pentru a verifica intersecția a două segmente
    auto linesIntersect = [&](const sf::Vector2f& a1, const sf::Vector2f& a2,
        const sf::Vector2f& b1, const sf::Vector2f& b2) -> bool {
            float denom = (a2.x - a1.x) * (b2.y - b1.y) - (a2.y - a1.y) * (b2.x - b1.x);
            if (denom == 0.f) return false; // Paralel

            float ua = ((b2.x - b1.x) * (a1.y - b1.y) - (b2.y - b1.y) * (a1.x - b1.x)) / denom;
            float ub = ((a2.x - a1.x) * (a1.y - b1.y) - (a2.y - a1.y) * (a1.x - b1.x)) / denom;

            return (ua >= 0.f && ua <= 1.f && ub >= 0.f && ub <= 1.f);
        };

    // Verificăm intersecția cu fiecare margine a dreptunghiului
    return linesIntersect(p1, p2, r1, r2) ||
        linesIntersect(p1, p2, r2, r3) ||
        linesIntersect(p1, p2, r3, r4) ||
        linesIntersect(p1, p2, r4, r1);
}

// Funcție pentru a calcula un traseu cu trei linii și două unghiuri de 90°, preferând direcția specificată
std::vector<sf::Vector2f> computeThreeLinePath(const sf::Vector2f& src, const sf::Vector2f& tgt, const std::vector<sf::FloatRect>& obstacles, bool preferDirectionUp = true) {
    std::vector<sf::Vector2f> path;
    path.clear();
    float padding = 50.f; // Valoare mai mare pentru linii mai lungi

    if (preferDirectionUp) {
        // Opțiunea 1: În sus -> Orizontal -> Înapoi în jos
        sf::Vector2f p1_up = src;
        p1_up.y -= padding;

        sf::Vector2f p2_up = sf::Vector2f(tgt.x, p1_up.y);

        bool path1Clear = true;
        for (const auto& rect : obstacles) {
            if (lineIntersectsRect(src, p1_up, rect) || lineIntersectsRect(p1_up, p2_up, rect) || lineIntersectsRect(p2_up, tgt, rect)) {
                path1Clear = false;
                break;
            }
        }

        if (path1Clear) {
            if (!path.empty() && path.size() > 3) {
                path.erase(path.begin() + 3, path.end()); // Eliminăm toate punctele redundante după al treilea segment
            }
            return path;
        }

        // Opțiunea 2: În jos -> Orizontal -> Înapoi în sus
        sf::Vector2f p1_down = src;
        p1_down.y += padding;

        sf::Vector2f p2_down = sf::Vector2f(tgt.x, p1_down.y);

        bool path2Clear = true;
        for (const auto& rect : obstacles) {
            if (lineIntersectsRect(src, p1_down, rect) || lineIntersectsRect(p1_down, p2_down, rect) || lineIntersectsRect(p2_down, tgt, rect)) {
                path2Clear = false;
                break;
            }
        }

        if (path2Clear) {
            path = { src, p1_down, p2_down, tgt };
            return path;
        }
    }
    else {
        // Opțiunea 1: În jos -> Orizontal -> Înapoi în sus
        sf::Vector2f p1_down = src;
        p1_down.y += padding;

        sf::Vector2f p2_down = sf::Vector2f(tgt.x, p1_down.y);

        bool path1Clear = true;
        for (const auto& rect : obstacles) {
            if (lineIntersectsRect(src, p1_down, rect) || lineIntersectsRect(p1_down, p2_down, rect) || lineIntersectsRect(p2_down, tgt, rect)) {
                path1Clear = false;
                break;
            }
        }

        if (path1Clear) {
            path = { src, p1_down, p2_down, tgt };
            return path;
        }

        // Opțiunea 2: În sus -> Orizontal -> Înapoi în jos
        sf::Vector2f p1_up = src;
        p1_up.y -= padding;

        sf::Vector2f p2_up = sf::Vector2f(tgt.x, p1_up.y);

        bool path2Clear = true;
        for (const auto& rect : obstacles) {
            if (lineIntersectsRect(src, p1_up, rect) || lineIntersectsRect(p1_up, p2_up, rect) || lineIntersectsRect(p2_up, tgt, rect)) {
                path2Clear = false;
                break;
            }
        }

        if (path2Clear) {
            path = { src, p1_up, p2_up, tgt };
            return path;
        }
    }

    if (path.size() > 3) {
        path.erase(path.begin() + 3, path.end()); // Păstrează doar primele 3 segmente
    }
    // Dacă nici una dintre opțiuni nu este clară, returnează un traseu L-shaped simplu
    sf::Vector2f waypoint = sf::Vector2f(src.x, tgt.y);
    path = { src, waypoint, tgt };
    return path;
}

// Funcție pentru a calcula un traseu de la src la tgt evitând obstacolele
std::vector<sf::Vector2f> computePath(const sf::Vector2f& src, const sf::Vector2f& tgt, const std::vector<sf::FloatRect>& obstacles) {
    // Verificăm dacă linia dreaptă este clară
    bool straightClear = true;
    for (const auto& rect : obstacles) {
        if (lineIntersectsRect(src, tgt, rect)) {
            straightClear = false;
            break;
        }
    }
    if (straightClear) {
        return { src, tgt };
    }

    // Dacă linia dreaptă intersectează, identificăm obstacolul
    std::vector<sf::FloatRect> intersectedObstacles;
    for (const auto& rect : obstacles) {
        if (lineIntersectsRect(src, tgt, rect)) {
            intersectedObstacles.push_back(rect);
        }
    }

    if (intersectedObstacles.empty()) {
        return { src, tgt };
    }

    // Luăm primul obstacol intersectat
    sf::FloatRect obstacle = intersectedObstacles[0];

    // Decidem să evităm obstacolul fie deasupra, fie dedesubt
    bool goAbove = src.y < obstacle.top + obstacle.height / 2.f;

    float padding = 10.f; // Padding pentru a nu atinge marginea hitbox-ului
    sf::Vector2f waypoint;

    if (goAbove) {
        waypoint = sf::Vector2f(src.x, obstacle.top - padding);
    }
    else {
        waypoint = sf::Vector2f(src.x, obstacle.top + obstacle.height + padding);
    }

    // Verificăm dacă traseul prin waypoint este clar
    bool pathClear = true;
    for (const auto& rect : obstacles) {
        if (lineIntersectsRect(src, waypoint, rect) || lineIntersectsRect(waypoint, tgt, rect)) {
            pathClear = false;
            break;
        }
    }

    if (pathClear) {
        return { src, waypoint, tgt };
    }

    // Dacă nu, încercăm un waypoint orizontal
    if (src.x < obstacle.left + obstacle.width / 2.f) {
        waypoint = sf::Vector2f(obstacle.left - padding, src.y);
    }
    else {
        waypoint = sf::Vector2f(obstacle.left + obstacle.width + padding, src.y);
    }

    // Verificăm dacă traseul prin waypoint orizontal este clar
    pathClear = true;
    for (const auto& rect : obstacles) {
        if (lineIntersectsRect(src, waypoint, rect) || lineIntersectsRect(waypoint, tgt, rect)) {
            pathClear = false;
            break;
        }
    }

    if (pathClear) {
        return { src, waypoint, tgt };
    }

    // Ca fallback, returnăm linia dreaptă
    return { src, tgt };
}

std::vector<sf::Vector2f> computePathWithRightAngles(const sf::Vector2f& src, const sf::Vector2f& tgt, const std::vector<sf::FloatRect>& obstacles) {
    std::vector<sf::Vector2f> path;

    // Încercăm orizontal apoi vertical
    sf::Vector2f h1(tgt.x, src.y);
    bool pathClear = true;
    for (const auto& rect : obstacles) {
        if (lineIntersectsRect(src, h1, rect) || lineIntersectsRect(h1, tgt, rect)) {
            pathClear = false;
            break;
        }
    }
    if (pathClear) {
        if (!path.empty() && path.back() == src) {
            path.pop_back(); // Elimină punctele redundante
        }
        sf::Vector2f p1_up = src;
        sf::Vector2f p2_up = tgt;
        path = { src, p1_up, p2_up, tgt };
        if (!path.empty() && path.size() > 3) {
            path.erase(path.begin() + 3, path.end());
        }
        if (!path.empty() && path.size() > 3) {
            path.erase(path.begin() + 3, path.end()); // Asigură-te că păstrăm doar primele 3 puncte ale traseului
        }
        return path;
    }

    // Încercăm vertical apoi orizontal
    sf::Vector2f v1(src.x, tgt.y);
    pathClear = true;
    for (const auto& rect : obstacles) {
        if (lineIntersectsRect(src, v1, rect) || lineIntersectsRect(v1, tgt, rect)) {
            pathClear = false;
            break;
        }
    }
    if (pathClear) {
        path.push_back(src);
        path.push_back(v1);
        path.push_back(tgt);
        return path;
    }

    // Necesită un traseu mai complex
    // Implementăm un algoritm simplu de căutare a traseului cu cotituri de 90 de grade

    // Începem de la src
    sf::Vector2f current = src;

    // Vom construi traseul deplasându-ne în direcții orizontale și verticale
    std::vector<sf::Vector2f> queue;
    std::set<sf::Vector2f, Vector2fCompare> visited;
    std::map<sf::Vector2f, sf::Vector2f, Vector2fCompare> cameFrom;

    queue.push_back(current);
    visited.insert(current);

    bool found = false;

    while (!queue.empty()) {
        current = queue.front();
        queue.erase(queue.begin());

        if (std::abs(current.x - tgt.x) < 1.f && std::abs(current.y - tgt.y) < 1.f) {
            // Am ajuns la țintă
            found = true;
            break;
        }

        // Explorăm vecinii în cele patru direcții
        std::vector<sf::Vector2f> neighbors = {
            { current.x + 10.f, current.y },
            { current.x - 10.f, current.y },
            { current.x, current.y + 10.f },
            { current.x, current.y - 10.f }
        };

        for (auto& neighbor : neighbors) {
            if (visited.find(neighbor) != visited.end())
                continue;

            // Verificăm dacă deplasarea de la current la neighbor este blocată
            bool blocked = false;
            for (const auto& rect : obstacles) {
                if (lineIntersectsRect(current, neighbor, rect)) {
                    blocked = true;
                    break;
                }
            }
            if (!blocked) {
                queue.push_back(neighbor);
                visited.insert(neighbor);
                cameFrom[neighbor] = current;
            }
        }
    }

    if (found) {
        // Reconstruim traseul
        sf::Vector2f step = tgt;
        path.push_back(step);

        while (step != src) {
            auto it = cameFrom.find(step);
            if (it == cameFrom.end())
                break;
            step = it->second;
            path.push_back(step);
        }
        std::reverse(path.begin(), path.end());
    }
    else {
        // Nu am putut găsi un traseu, returnăm linia directă
        path = { src, tgt };
    }

    return path;
}

// Funcție auxiliară pentru a verifica dacă un port este deja conectat
bool isPortConnected(const std::vector<Connection>& connections, std::shared_ptr<DraggableSquare> component, int portIndex) {
    for (const auto& conn : connections) {
        if ((conn.sourceComponent == component && conn.sourcePortIndex == portIndex) ||
            (conn.targetComponent == component && conn.targetPortIndex == portIndex)) {
            return true;
        }
    }
    return false;
}

// Funcție pentru a calcula distanța de la un punct la o linie (segment) definită de p1 și p2
float pointToLineDistance(const sf::Vector2f& point, const sf::Vector2f& p1, const sf::Vector2f& p2) {
    float A = point.x - p1.x;
    float B = point.y - p1.y;
    float C = p2.x - p1.x;
    float D = p2.y - p1.y;

    float dot = A * C + B * D;
    float len_sq = C * C + D * D;
    float param = -1.f;

    if (len_sq != 0.f) // in case of 0 length line
        param = dot / len_sq;

    float xx, yy;

    if (param < 0.f) {
        xx = p1.x;
        yy = p1.y;
    }
    else if (param > 1.f) {
        xx = p2.x;
        yy = p2.y;
    }
    else {
        xx = p1.x + param * C;
        yy = p1.y + param * D;
    }

    float dx = point.x - xx;
    float dy = point.y - yy;

    return sqrt(dx * dx + dy * dy);
}

bool run = true, isInMenu = true;
bool loadProject = false;
bool isValoriMenuOpen = false;  // Flag for the menu state
sf::RectangleShape valoriMenu;  // Rectangle for the menu
sf::Text valoriTitle;           // Title text for the menu
sf::RectangleShape valoriCloseButton; // Close button for the menu
std::vector<sf::Text> valoriLabels;  // Labels for the menu values
std::vector<sf::Text> dimensions;
std::vector<sf::RectangleShape> valoriInputBoxes;  // Input boxes for values
std::vector<sf::Text> valoriUnits;   // Units for the values
int selectedInputBox = -1;  // Keeps track of the selected input box for editing
std::string saveAsFileName;
int main() {
    while (run) {
        if (isInMenu) {
            // **Modified: Retrieve desktop resolution dynamically**
            sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
            SCREEN_WIDTH = desktop.width;
            SCREEN_HEIGHT = desktop.height;

            // **Modified: Initialize window in fullscreen mode using desktop resolution**
            sf::RenderWindow window(desktop, "Electron", sf::Style::Fullscreen);
            initializeWindow(window);

            sf::Font sansSerifFont = loadSansSerifFont();
            drawTitleScreen(window, sansSerifFont);

            Button mainMenuButtons[MAX_BUTTONS];
            Button settingsButtons[MAX_BUTTONS];
            Button informationButtons[1];
            Button newComponentButtons[MAX_BUTTONS];
            Button cancelConfirmationButtons[MAX_BUTTONS];

            int mainMenuButtonCount = 0;
            int settingsButtonCount = 0;
            int informationButtonCount = 0;
            int newComponentButtonCount = 0;
            int cancelConfirmationButtonCount = 0;


            componentNameBox.isActive = false;

            createMainMenuButtons(mainMenuButtons, mainMenuButtonCount, sansSerifFont);
            createSettingsButtons(settingsButtons, settingsButtonCount, sansSerifFont);
            createInformationButtons(informationButtons, informationButtonCount, sansSerifFont);
            createNewComponentButtons(newComponentButtons, newComponentButtonCount, sansSerifFont);
            createCancelConfirmationButtons(cancelConfirmationButtons, cancelConfirmationButtonCount, sansSerifFont);

            sf::Text infoContent = loadInformationContent(sansSerifFont, "Information.txt");

            bool running = true;
            ApplicationPage currentPage = ApplicationPage::MainMenu;
            ApplicationPage previousPage = currentPage;

            sf::Clock clock;


            while (running && isInMenu) {
                sf::Time elapsed = clock.restart();

                // Poll events
                sf::Event event;
                while (window.pollEvent(event)) {
                    handleGlobalEvents(event, running);

                    // Handle page-specific interactions based on the current page
                    if (currentPage == ApplicationPage::MainMenu) {
                        handleMainMenuInteractions(event, window, mainMenuButtons, mainMenuButtonCount, running, currentPage);
                    }
                    else if (currentPage == ApplicationPage::Settings) {
                        handleSettingsInteractions(event, window, settingsButtons, settingsButtonCount, currentPage);
                    }
                    else if (currentPage == ApplicationPage::Information) {
                        handleInformationInteractions(event, window, informationButtons, informationButtonCount, currentPage);
                    }
                    else if (currentPage == ApplicationPage::NewComponent) {
                        handleNewComponentInteractions(event, window, newComponentButtons, newComponentButtonCount, currentPage, componentNameBox);
                        if (componentNameBox.isActive) {
                            handleTextInput(componentNameBox, event);
                            handleTextBoxScrolling(componentNameBox, event);
                        }
                    }
                    else if (currentPage == ApplicationPage::CancelConfirmation) {
                        handleCancelConfirmationInteractions(event, window, cancelConfirmationButtons, cancelConfirmationButtonCount, currentPage);
                    }
                }

                // --- STEP 1: Handle page-specific updates ---
                if (currentPage == ApplicationPage::MainMenu) {
                    updateButtonColors(mainMenuButtons, mainMenuButtonCount, window);
                }
                else if (currentPage == ApplicationPage::Settings) {
                    updateButtonColors(settingsButtons, settingsButtonCount, window);
                }
                else if (currentPage == ApplicationPage::Information) {
                    updateButtonColors(informationButtons, informationButtonCount, window);
                }
                else if (currentPage == ApplicationPage::NewComponent) {
                    updateButtonColors(newComponentButtons, newComponentButtonCount, window);
                    updateTextInputBox(componentNameBox, sansSerifFont, elapsed.asSeconds());
                }
                else if (currentPage == ApplicationPage::CancelConfirmation) {
                    updateButtonColors(cancelConfirmationButtons, cancelConfirmationButtonCount, window);
                }

                // --- STEP 2: Check if we just LEFT the NewComponent page -> Clear text ---
                if (previousPage == ApplicationPage::NewComponent && currentPage != ApplicationPage::NewComponent) {
                    componentNameBox.inputText.clear();
                    componentNameBox.displayText.setString("");
                    componentNameBox.selectionStart = 0;
                    componentNameBox.selectionEnd = 0;
                    componentNameBox.scrollOffset = 0.0f;
                }

                // --- STEP 3: Render everything ---
                window.clear(sf::Color(28, 28, 28));

                if (currentPage == ApplicationPage::MainMenu) {
                    drawMainMenu(window, sansSerifFont, mainMenuButtons, mainMenuButtonCount);
                }
                else if (currentPage == ApplicationPage::Settings) {
                    drawSettingsMenu(window, sansSerifFont, settingsButtons, settingsButtonCount);
                }
                else if (currentPage == ApplicationPage::Information) {
                    drawInformationPage(window, sansSerifFont, infoContent, informationButtons, informationButtonCount);
                }
                else if (currentPage == ApplicationPage::NewComponent) {
                    drawNewComponentPage(window, sansSerifFont, newComponentButtons, newComponentButtonCount, componentNameBox);
                    if (refreshPiece != nullptr) { // Added null check to prevent potential crashes
                        refreshPiece->drawPiece(window, sf::Vector2f(SCREEN_WIDTH * 0.25f, SCREEN_HEIGHT * 0.49f), 120.0f);
                    }
                }
                else if (currentPage == ApplicationPage::CancelConfirmation) {
                    drawCancelConfirmationPopup(window, sansSerifFont, cancelConfirmationButtons, cancelConfirmationButtonCount);
                }

                window.display();

                // --- STEP 4: Update previousPage ---
                previousPage = currentPage;
            }

            // **Modified: Clean up dynamically allocated memory**
            if (refreshPiece != nullptr) {
                delete refreshPiece;
            }
        }
        else {
            std::vector<StaticSquare> staticSquares;
            std::vector<std::shared_ptr<DraggableSquare>> draggableSquares;

            // Vector pentru a stoca toate conexiunile
            std::vector<Connection> connections;

            // Variabile pentru gestionarea conexiunilor în curs
            bool isConnecting = false;
            std::shared_ptr<DraggableSquare> connectionSource = nullptr;
            int connectionSourcePort = -1;

            // Linie temporară de desenat în timpul conexiunii
            sf::VertexArray tempLine(sf::Lines, 2);

            //FISIERELE
            std::ifstream inFile("piece.txt");

            std::deque<std::string> pieceFiles;
            std::string auxPiece;
            while (inFile >> auxPiece) {
                pieceFiles.push_back(auxPiece);
            }
            inFile.close();

            // Dimensiunile ecranului
            sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
            int screenWidth = desktop.width;
            int screenHeight = desktop.height;

            // Creăm fereastra fullscreen
            sf::RenderWindow window(sf::VideoMode(screenWidth, screenHeight), "Fullscreen Menu Example", sf::Style::Fullscreen);

            // View-ul principal pentru zoom și dragging
            sf::View view = window.getDefaultView();

            // Variabile pentru dragging
            bool isDragging = false;
            sf::Vector2i dragStart;

            // Factor de zoom
            float zoomFactor = 1.f;
            const float minZoom = 0.5f; // Zoom minim
            const float maxZoom = 2.0f; // Zoom maxim

            sf::RectangleShape saveAsPopup;
            sf::Text saveAsTitle;
            sf::Text saveAsTextBoxText;
            std::string saveAsTextBoxInput = "";
            sf::RectangleShape saveAsCloseButton;
            bool isSaveAsPopupVisible = false;

            sf::RectangleShape saveAsCursor;
            float cursorBlinkTimer = 0.0f;
            bool isCursorVisible = true;

            // Creăm header-ul
            sf::RectangleShape header;
            header.setSize(sf::Vector2f(static_cast<float>(screenWidth), static_cast<float>(screenHeight) / 8)); // 1 din 8
            header.setFillColor(sf::Color(100, 100, 100)); // Gri opac
            header.setPosition(0, 0);

            // Creăm cele 5 pătrate
            squareSize = header.getSize().y * 3.f / 4.f; // 3 din 4 din înălțimea header-ului
            float spacing = 10.f; // Spațiu între pătrate

            // Poziționarea primului pătrat
            float firstSquareX = (screenWidth - 5 * squareSize - 4 * spacing) / 2; // Centrat pe orizontală

            // Pătratele
            sf::RectangleShape square1(sf::Vector2f(squareSize, squareSize));
            sf::RectangleShape square2(sf::Vector2f(squareSize, squareSize));
            sf::RectangleShape square3(sf::Vector2f(squareSize, squareSize));
            sf::RectangleShape square4(sf::Vector2f(squareSize, squareSize));
            sf::RectangleShape square5(sf::Vector2f(squareSize, squareSize));

            // Setăm culoarea neagră pentru pătrate
            square1.setFillColor(sf::Color::Black);
            square2.setFillColor(sf::Color::Black);
            square3.setFillColor(sf::Color::Black);
            square4.setFillColor(sf::Color::Black);
            square5.setFillColor(sf::Color::Black);

            // Poziționăm pătratele pe orizontală
            square1.setPosition(firstSquareX, header.getSize().y / 2 - squareSize / 2);  // Centrat pe verticală
            square2.setPosition(firstSquareX + squareSize + spacing, header.getSize().y / 2 - squareSize / 2);
            square3.setPosition(firstSquareX + 2 * (squareSize + spacing), header.getSize().y / 2 - squareSize / 2);
            square4.setPosition(firstSquareX + 3 * (squareSize + spacing), header.getSize().y / 2 - squareSize / 2);
            square5.setPosition(firstSquareX + 4 * (squareSize + spacing), header.getSize().y / 2 - squareSize / 2);
            for (int i = 0; i < 3; ++i) {
                if (i < pieceFiles.size()) {
                    auto pieceDrawer = std::make_shared<PieceDrawer>(pieceFiles[i]);
                    staticSquares.emplace_back(pieceDrawer, sf::Vector2f(
                        square2.getPosition().x + i * (squareSize + spacing) + squareSize / 2.f - 10.f, // Ajustează offset-ul dacă este necesar
                        square2.getPosition().y + squareSize / 2.f - 10.f // Ajustează offset-ul dacă este necesar
                    ));
                }
            }
            // Creăm butonul (cele 3 linii)
            float buttonWidth = 40.f;
            float buttonHeight = 30.f;
            float lineThickness = 3.f;
            float buttonX = 60.f; // Poziționat mai la dreapta
            float buttonY = header.getSize().y / 2 - buttonHeight / 2; // Centrat pe verticală în header

            sf::RectangleShape line1(sf::Vector2f(buttonWidth, lineThickness));
            sf::RectangleShape line2(sf::Vector2f(buttonWidth, lineThickness));
            sf::RectangleShape line3(sf::Vector2f(buttonWidth, lineThickness));
            line1.setPosition(buttonX, buttonY);
            line2.setPosition(buttonX, buttonY + 10.f);
            line3.setPosition(buttonX, buttonY + 20.f);
            line1.setFillColor(sf::Color::Black);
            line2.setFillColor(sf::Color::Black);
            line3.setFillColor(sf::Color::Black);

            // Creăm meniul gri
            sf::RectangleShape menu;
            menu.setSize(sf::Vector2f(static_cast<float>(screenWidth) / 5, static_cast<float>(screenHeight)));
            menu.setFillColor(sf::Color(160, 160, 160)); // Gri
            menu.setPosition(0, 0);
            bool isMenuOpen = false; // Starea meniului

            // Creăm butonul "X" pentru închiderea meniului
            float closeButtonSize = 40.f; // Dimensiunea butonului "X"
            sf::RectangleShape closeButton(sf::Vector2f(closeButtonSize, closeButtonSize));
            closeButton.setFillColor(sf::Color(44, 48, 85));
            float closeButtonX = menu.getSize().x - closeButtonSize - 10.f; // Poziționat în partea dreaptă sus
            float closeButtonY = 10.f; // 10px de la marginea sus
            closeButton.setPosition(closeButtonX, closeButtonY);

            // Creăm liniile care formează "X"-ul în butonul de închidere
            sf::VertexArray xLines(sf::Lines, 4);
            float lineThicknessX = 5.f; // Grosimea liniei pentru X

            // Linia 1 a X-ului
            xLines[0].position = sf::Vector2f(closeButtonX + 5.f, closeButtonY + 5.f);
            xLines[0].color = sf::Color::White;
            xLines[1].position = sf::Vector2f(closeButtonX + closeButtonSize - 5.f, closeButtonY + closeButtonSize - 5.f);
            xLines[1].color = sf::Color::White;

            // Linia 2 a X-ului
            xLines[2].position = sf::Vector2f(closeButtonX + closeButtonSize - 5.f, closeButtonY + 5.f);
            xLines[2].color = sf::Color::White;
            xLines[3].position = sf::Vector2f(closeButtonX + 5.f, closeButtonY + closeButtonSize - 5.f);
            xLines[3].color = sf::Color::White;

            // Crearea butoanelor pentru meniu
            sf::RectangleShape backButton(sf::Vector2f(menu.getSize().x - 20.f, 50.f));
            backButton.setFillColor(sf::Color(44, 48, 85)); // Gri deschis
            backButton.setPosition(10.f, 60.f); // Poziționat mai sus

            sf::RectangleShape saveButton(sf::Vector2f(menu.getSize().x - 20.f, 50.f));
            saveButton.setFillColor(sf::Color(44, 48, 85)); // Gri deschis
            saveButton.setPosition(10.f, 120.f);

            sf::RectangleShape saveAsButton(sf::Vector2f(menu.getSize().x - 20.f, 50.f));
            saveAsButton.setFillColor(sf::Color(44, 48, 85)); // Gri deschis
            saveAsButton.setPosition(10.f, 180.f);

            sf::RectangleShape resetButton(sf::Vector2f(menu.getSize().x - 20.f, 50.f));
            resetButton.setFillColor(sf::Color(44, 48, 85)); // Gri deschis
            resetButton.setPosition(10.f, 240.f);

            // Creăm butonul de delete în colțul din dreapta sus
            sf::RectangleShape deleteButton(sf::Vector2f(100.f, 40.f));
            deleteButton.setFillColor(sf::Color(44, 48, 85));
            deleteButton.setPosition(static_cast<float>(screenWidth) - 110.f, 10.f);

            // Creăm textul pentru butoane
            sf::Font font;
            // Folosește calea corectă la fontul tău
            if (!font.loadFromFile("C:\\Windows\\Fonts\\arial.ttf")) {  // Calea absolută către Arial
                std::cerr << "Failed to load font!" << std::endl;
                return -1; // Ieşim dacă fontul nu poate fi încărcat
            }

            sf::Text deleteButtonText("Delete", font, 20);
            deleteButtonText.setFillColor(sf::Color::White);
            deleteButtonText.setPosition(deleteButton.getPosition().x + 20.f, deleteButton.getPosition().y + 4.f);

            sf::Text backText("Back", font, 20);
            backText.setPosition(backButton.getPosition().x + 10.f, backButton.getPosition().y + 10.f);

            sf::Text saveText("Save", font, 20);
            saveText.setPosition(saveButton.getPosition().x + 10.f, saveButton.getPosition().y + 10.f);

            sf::Text saveAsText("Save As", font, 20);
            saveAsText.setPosition(saveAsButton.getPosition().x + 10.f, saveAsButton.getPosition().y + 10.f);

            sf::Text resetText("Reset", font, 20);
            resetText.setPosition(resetButton.getPosition().x + 10.f, resetButton.getPosition().y + 10.f);

            // Textul pentru numerele din pătrate
            sf::Text squareNumbers[3]; // Pentru pătratele 2, 3 și 4
            for (int i = 0; i < 3; ++i) {
                squareNumbers[i].setFont(font);
                squareNumbers[i].setCharacterSize(20);
                squareNumbers[i].setFillColor(sf::Color::White);
            }

            // Variabile pentru popup
            sf::RectangleShape popup;
            sf::Text popupText;
            sf::Clock popupClock;
            bool showPopup = false;
            std::string popupMessage;

            // Creăm butonul "X" pentru popup
            // Creăm butonul "X" pentru popup
            sf::RectangleShape popupCloseButton(sf::Vector2f(30.f, 30.f)); // Dimensiunea butonului "X"
            popupCloseButton.setFillColor(sf::Color(44, 48, 85));

            // Creăm liniile pentru "X"-ul din buton
            sf::VertexArray popupXLines(sf::Lines, 4);
            popupXLines[0].position = sf::Vector2f(popupCloseButton.getPosition().x + 5.f, popupCloseButton.getPosition().y + 5.f);
            popupXLines[0].color = sf::Color::White;
            popupXLines[1].position = sf::Vector2f(popupCloseButton.getPosition().x + popupCloseButton.getSize().x - 5.f, popupCloseButton.getPosition().y + popupCloseButton.getSize().y - 5.f);
            popupXLines[1].color = sf::Color::White;

            popupXLines[2].position = sf::Vector2f(popupCloseButton.getPosition().x + popupCloseButton.getSize().x - 5.f, popupCloseButton.getPosition().y + 5.f);
            popupXLines[2].color = sf::Color::White;
            popupXLines[3].position = sf::Vector2f(popupCloseButton.getPosition().x + 5.f, popupCloseButton.getPosition().y + popupCloseButton.getSize().y - 5.f);
            popupXLines[3].color = sf::Color::White;

            sf::RectangleShape sheetBorder(sf::Vector2f(sheetBounds.width, sheetBounds.height));
            sheetBorder.setPosition(sheetBounds.left, sheetBounds.top);
            sheetBorder.setFillColor(sf::Color::Transparent);
            sheetBorder.setOutlineColor(sf::Color::Red);
            sheetBorder.setOutlineThickness(5.f);

            // Initialize Valori menu
            valoriMenu.setSize(sf::Vector2f(400.f, 600.f));
            valoriMenu.setFillColor(sf::Color(200, 200, 200));
            valoriMenu.setOutlineColor(sf::Color::Black);
            valoriMenu.setOutlineThickness(2.f);

            // Position and title for the menu
            valoriMenu.setPosition(200.f, 100.f);
            valoriTitle.setFont(font);
            valoriTitle.setString("Valori:");
            valoriTitle.setCharacterSize(24);
            valoriTitle.setFillColor(sf::Color::Black);
            valoriTitle.setPosition(valoriMenu.getPosition().x + 20.f, valoriMenu.getPosition().y + 10.f);

            // Close button
            valoriCloseButton.setSize(sf::Vector2f(30.f, 30.f));
            valoriCloseButton.setFillColor(sf::Color::Red);
            valoriCloseButton.setPosition(valoriMenu.getPosition().x + valoriMenu.getSize().x - 40.f, valoriMenu.getPosition().y + 10.f);

            // Initialize labels and input boxes for values
            std::vector<std::string> valueLabels = {
                "Factor de scalare -> ", "Factor de rotatie -> ", "Rezistenta -> ", "Tensiune -> ",
                "Curent electric -> ", "Putere -> ", "Capacitate -> ", "Inductanta -> ",
                "Cadere de tensiune pe dioda -> ", "Frecventa -> ", "Energie electrica -> ", "Rezistenta termica -> "
            };

            std::vector<std::string> valueUnits = {
                ".f", "°", "Ω", "V", "A", "W", "F", "H", "V", "Hz", "J", "K/W"
            };

            float yOffset = 50.f;
            for (size_t i = 0; i < valueLabels.size(); ++i) {
                sf::Text label;
                label.setFont(font);
                label.setString(valueLabels[i]);
                label.setCharacterSize(18);
                label.setFillColor(sf::Color::Black);
                label.setPosition(valoriMenu.getPosition().x + 280.f, valoriMenu.getPosition().y + yOffset);

                sf::RectangleShape inputBox(sf::Vector2f(50.f, 30.f));
                inputBox.setFillColor(sf::Color::White);
                inputBox.setOutlineColor(sf::Color::Black);
                inputBox.setOutlineThickness(1.f);
                inputBox.setPosition(valoriMenu.getPosition().x + 280.f, valoriMenu.getPosition().y + yOffset - 5.f);

                sf::Text unit;
                unit.setFont(font);
                unit.setString(valueUnits[i]);
                unit.setCharacterSize(18);
                unit.setFillColor(sf::Color::Black);
                unit.setPosition(inputBox.getPosition().x + inputBox.getSize().x + 10.f, inputBox.getPosition().y);

                sf::Text dimension;
                dimension.setFont(font);
                dimension.setString(valueLabels[i]);
                dimension.setCharacterSize(18);
                dimension.setFillColor(sf::Color::Black);
                dimension.setPosition(valoriMenu.getPosition().x + 20.f, valoriMenu.getPosition().y + yOffset - 5.f);

                valoriLabels.push_back(label);
                valoriInputBoxes.push_back(inputBox);
                valoriUnits.push_back(unit);
                dimensions.push_back(dimension);
                yOffset += 40.f;
            }

            // Inițializarea ferestrei popup
            saveAsPopup.setSize(sf::Vector2f(400.f, 200.f));
            saveAsPopup.setFillColor(sf::Color(200, 200, 200));
            saveAsPopup.setOutlineColor(sf::Color::Black);
            saveAsPopup.setOutlineThickness(2.f);
            saveAsPopup.setPosition(static_cast<float>(screenWidth) / 2 - 200.f, static_cast<float>(screenHeight) / 2 - 100.f);

            // Text pentru titlu
            saveAsTitle.setFont(font);
            saveAsTitle.setCharacterSize(20);
            saveAsTitle.setFillColor(sf::Color::Black);
            saveAsTitle.setString("Denumeste-ti proiectul");
            saveAsTitle.setPosition(saveAsPopup.getPosition().x + 10.f, saveAsPopup.getPosition().y + 10.f);

            // Text pentru textbox
            saveAsTextBoxText.setFont(font);
            saveAsTextBoxText.setCharacterSize(18);
            saveAsTextBoxText.setFillColor(sf::Color::Black);
            saveAsTextBoxText.setString("");
            saveAsTextBoxText.setPosition(saveAsPopup.getPosition().x + 10.f, saveAsPopup.getPosition().y + 60.f);

            // Textbox cursor
            saveAsCursor.setSize(sf::Vector2f(2.f, 20.f));
            saveAsCursor.setFillColor(sf::Color::Black);
            saveAsCursor.setPosition(saveAsTextBoxText.getPosition().x, saveAsTextBoxText.getPosition().y + 5.f);

            // Buton "X" de închidere
            saveAsCloseButton.setSize(sf::Vector2f(30.f, 30.f));
            saveAsCloseButton.setFillColor(sf::Color::Red);
            saveAsCloseButton.setPosition(saveAsPopup.getPosition().x + saveAsPopup.getSize().x - 40.f, saveAsPopup.getPosition().y + 10.f);

            // Loop-ul principal
            bool ok = true;
            bool okSaveAs = true;
            while (window.isOpen() && !isInMenu) {
                sf::Event event;
                while (window.pollEvent(event)) {
                    // Zoom cu scroll-ul mouse-ului
                    if (loadProject) {
                        std::ifstream inFile(saveAsFileName); 
                        int nrDragSqr, idC, nrConne, idCIn, idCOut, indexIn, indexOut;
                        float posX, posY, localscaleC, rotationAngleC;
                        std::string fileNameForDrawer;
                        inFile >> nrDragSqr;
                        for (int i = 1;i <= nrDragSqr;i++) {
                            inFile >> idC >> posX >> posY >> fileNameForDrawer >> localscaleC >> rotationAngleC;
                            auto prototipDrawer = std::make_shared<PieceDrawer>(fileNameForDrawer);
                            DraggableSquare* prototip = new DraggableSquare(prototipDrawer, sf::Vector2f(posX, posY));
                            prototip->id = idC;
                            prototip->localScale = localscaleC;
                            prototip->rotationAngle = rotationAngleC;
                            for (int i = 0;i < prototip->componentValues.size();i++)
                                inFile >> prototip->componentValues[i];
                            draggableSquares.emplace_back(prototip);
                        }
                        inFile >> nrConne;
                        std::shared_ptr<DraggableSquare> auxIn, auxOut;
                        for (int i = 1;i <= nrConne;i++) {
                            inFile >> idCIn >> indexIn >> idCOut >> indexOut;
                            int ok_id_i = false, ok_id_o = false;
                            for (int j = 0;j < draggableSquares.size() && (!ok_id_i || !ok_id_o);j++) {
                                if (draggableSquares[j]->id == idCIn) {
                                    ok_id_i = true;
                                    auxIn = draggableSquares[j];
                                }
                                if (draggableSquares[j]->id == idCOut) {
                                    ok_id_o = true;
                                    auxOut = draggableSquares[j];
                                }
                            }
                            connections.emplace_back(Connection(auxIn, indexIn, auxOut, indexOut));
                        }
                        inFile >> idAct;
                        inFile.close();
                        loadProject = false;
                    }
                    if (event.type == sf::Event::MouseWheelScrolled) {
                        if (event.mouseWheelScroll.delta > 0 && zoomFactor > minZoom) {
                            zoomFactor -= 0.1; // Zoom in
                        }
                        else if (event.mouseWheelScroll.delta < 0 && zoomFactor < maxZoom) {
                            zoomFactor += 0.1; // Zoom out
                        }
                        view.setSize(window.getDefaultView().getSize());
                        view.zoom(zoomFactor);
                    }

                    // Închidem aplicația la apăsarea tastei Escape sau la închiderea ferestrei
                    if (event.type == sf::Event::Closed ||
                        (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)) {
                        run = false;
                        window.close();
                    }

                    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::I) {
                        isValoriMenuOpen = false; // Implicit închis
                        std::cout << 'a';
                        for (auto& draggable : draggableSquares) {
                            std::cout << 'b';
                            if (draggable->hitboxVisible) { // Dacă hitbox-ul este vizibil
                                isValoriMenuOpen = true;
                                std::cout << 'c';
                                // Obține valorile componentei
                                auto& values = draggable->componentValues;
                                std::cout << 'd';
                                // Populează meniul cu valorile componentei
                                for (size_t i = 0; i < valoriLabels.size(); ++i) {
                                    valoriLabels[i].setString(values[i]);
                                }
                                std::cout << 'e';
                                break; // Prima componentă vizibilă găsită
                            }
                        }
                    }

                    // Detectăm clicuri ale mouse-ului
                    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                        // Conversia poziției mouse-ului în coordonate ale lumii
                        sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
                        sf::Vector2f mousePosF = window.mapPixelToCoords(pixelPos, view);
                        sf::Vector2f uiMousePos = window.mapPixelToCoords(pixelPos, window.getDefaultView());

                        if (deleteButton.getGlobalBounds().contains(uiMousePos)) {
                            // Caută componenta cu hitbox-ul vizibil și șterge-o împreună cu conexiunile sale
                            for (auto it = draggableSquares.begin(); it != draggableSquares.end();) {
                                if ((*it)->hitboxVisible) {
                                    // Ștergem conexiunile legate de această componentă
                                    connections.erase(
                                        std::remove_if(connections.begin(), connections.end(),
                                            [&](const Connection& conn) {
                                                return conn.sourceComponent == *it || conn.targetComponent == *it;
                                            }),
                                        connections.end()
                                    );

                                    // Ștergem componenta
                                    it = draggableSquares.erase(it);
                                }
                                else {
                                    ++it;
                                }
                            }
                        }
                        if (!isMenuOpen) {
                            // Verificăm clic pe pătratele săgeată (stânga și dreapta)
                            if (square1.getGlobalBounds().contains(uiMousePos)) {
                                // Apăsare pe săgeata stângă
                                std::string last = pieceFiles.back();
                                pieceFiles.pop_back();
                                pieceFiles.push_front(last);
                                // Actualizează staticSquares
                                staticSquares.clear();
                                for (int i = 0; i < 3; ++i) {
                                    if (i < pieceFiles.size()) {
                                        auto pieceDrawer = std::make_shared<PieceDrawer>(pieceFiles[i]);
                                        staticSquares.emplace_back(pieceDrawer, sf::Vector2f(
                                            square2.getPosition().x + i * (squareSize + spacing) + squareSize / 2.f - 10.f, // Ajustează offset-ul dacă este necesar
                                            square2.getPosition().y + squareSize / 2.f - 10.f // Ajustează offset-ul dacă este necesar
                                        ));
                                    }
                                }
                            }
                            if (square5.getGlobalBounds().contains(uiMousePos)) {
                                // Apăsare pe săgeata dreaptă
                                std::string first = pieceFiles.front();
                                pieceFiles.pop_front();
                                pieceFiles.push_back(first);
                                // Actualizează staticSquares
                                staticSquares.clear();
                                for (int i = 0; i < 3; ++i) {
                                    if (i < pieceFiles.size()) {
                                        auto pieceDrawer = std::make_shared<PieceDrawer>(pieceFiles[i]);
                                        staticSquares.emplace_back(pieceDrawer, sf::Vector2f(
                                            square2.getPosition().x + i * (squareSize + spacing) + squareSize / 2.f - 10.f, // Ajustează offset-ul dacă este necesar
                                            square2.getPosition().y + squareSize / 2.f - 10.f // Ajustează offset-ul dacă este necesar
                                        ));
                                    }
                                }
                            }
                            // Dacă meniul este închis, verificăm clic pe butonul de meniu (cele 3 linii)
                            if (uiMousePos.x >= buttonX && uiMousePos.x <= buttonX + buttonWidth &&
                                uiMousePos.y >= buttonY && uiMousePos.y <= buttonY + buttonHeight) {
                                isMenuOpen = true; // Deschidem meniul
                            }

                        }
                        else {
                            // Dacă meniul este deschis, verificăm clic pe butonul "X"
                            if (uiMousePos.x >= closeButtonX && uiMousePos.x <= closeButtonX + closeButton.getSize().x &&
                                uiMousePos.y >= closeButtonY && uiMousePos.y <= closeButtonY + closeButton.getSize().y) {
                                isMenuOpen = false; // Închidem meniul
                            }
                            // Verificăm clic pe butoanele din meniu
                            if (uiMousePos.x >= backButton.getPosition().x && uiMousePos.x <= backButton.getPosition().x + backButton.getSize().x &&
                                uiMousePos.y >= backButton.getPosition().y && uiMousePos.y <= backButton.getPosition().y + backButton.getSize().y) {
                                isInMenu = true;
                                loadProject = false;
                                popupMessage = "Ne intoarcem la pagina principala!";
                                showPopup = true;
                                popupClock.restart();
                                while (valoriLabels.size()) {
                                    valoriLabels.pop_back();
                                }
                                showPopup = true;
                                popupClock.restart();
                            }
                            if (uiMousePos.x >= saveButton.getPosition().x && uiMousePos.x <= saveButton.getPosition().x + saveButton.getSize().x &&
                                uiMousePos.y >= saveButton.getPosition().y && uiMousePos.y <= saveButton.getPosition().y + saveButton.getSize().y) {
                                std::string fileName = saveAsFileName;
                                std::ofstream outFile(fileName, std::ios::out | std::ios::trunc);
                                outFile << draggableSquares.size() << '\n';
                                for (int i = 0;i < draggableSquares.size();i++) {
                                    outFile << draggableSquares[i]->id << ' ' << draggableSquares[i]->position.x << ' ' << draggableSquares[i]->position.y << ' ' << draggableSquares[i]->pieceDrawer->fileNameForDrawer << ' ' << draggableSquares[i]->localScale << ' ' << draggableSquares[i]->rotationAngle << ' ';
                                    for (int j = 0;j < draggableSquares[i]->componentValues.size();j++)
                                        outFile << draggableSquares[i]->componentValues[j] << ' ';
                                    outFile << '\n';
                                }
                                outFile << connections.size() << '\n';
                                for (int i = 0;i < connections.size();i++)
                                    outFile << connections[i].sourceComponent->id << ' ' << connections[i].sourcePortIndex << ' ' << connections[i].targetComponent->id << ' ' << connections[i].targetPortIndex << '\n';
                                outFile << idAct;
                                outFile.close();
                                popupMessage = "Proiectul tau a fost salvat !";
                                showPopup = true;
                                popupClock.restart();
                            }
                            if (uiMousePos.x >= saveAsButton.getPosition().x && uiMousePos.x <= saveAsButton.getPosition().x + saveAsButton.getSize().x &&
                                uiMousePos.y >= saveAsButton.getPosition().y && uiMousePos.y <= saveAsButton.getPosition().y + saveAsButton.getSize().y) {
                                isSaveAsPopupVisible = true;
                                saveAsTextBoxInput = ""; // Resetează inputul
                                saveAsTextBoxText.setString("");
                            }
                            if (uiMousePos.x >= resetButton.getPosition().x && uiMousePos.x <= resetButton.getPosition().x + resetButton.getSize().x &&
                                uiMousePos.y >= resetButton.getPosition().y && uiMousePos.y <= resetButton.getPosition().y + resetButton.getSize().y) {
                                while (connections.size()) {
                                    connections.pop_back();
                                }
                                while (draggableSquares.size())
                                    draggableSquares.pop_back();
                                popupMessage = "Am resetat interfata de lucru pentru tine!";
                                showPopup = true;
                                popupClock.restart();
                            }
                            if (popupCloseButton.getGlobalBounds().contains(mousePosF)) {
                                showPopup = false; // Ascundem popup-ul
                            }
                        }

                        // Partea mea: Crearea unui DraggableSquare dintr-un StaticSquare la centrul view-ului
                        bool ok = false;
                        for (auto& staticSquare : staticSquares) {
                            if (staticSquare.isMouseOver(uiMousePos) && !ok) {
                                // Obține centrul view-ului
                                sf::Vector2f center = view.getCenter();

                                // Creează și adaugă DraggableSquare la centrul view-ului
                                draggableSquares.emplace_back(staticSquare.createDraggableSquare(center));

                                // Setează localScale la scale pentru a menține dimensiunea componentelor
                                draggableSquares.back()->localScale = 1.f;

                                ok = true;
                            }
                        }

                        // 1. Declară flag-ul înainte de loop
                        bool clickedOnAnyDraggable = false;

                        // 2. În interiorul loop-ului, când detectezi un DraggableSquare, setează flag-ul la true și gestionează dublu-click-ul
                        for (auto it = draggableSquares.rbegin(); it != draggableSquares.rend(); ++it) {
                            // Calculează bounding box-ul ajustat pentru scalare
                            float boxSize = squareSize * (*it)->localScale;
                            sf::FloatRect boundingBox((*it)->position.x, (*it)->position.y, boxSize, boxSize);

                            if (boundingBox.contains(mousePosF)) {
                                clickedOnAnyDraggable = true;

                                // Gestionare dublu-click
                                sf::Time now = (*it)->doubleClickClock.getElapsedTime();
                                if ((*it)->firstClick) {
                                    if ((now - (*it)->lastClickTime) < sf::milliseconds(300)) {
                                        // Dublu-click detectat
                                        (*it)->hitboxVisible = !(*it)->hitboxVisible;
                                        (*it)->firstClick = false;
                                    }
                                    else {
                                        // Reset și setează primul click
                                        (*it)->firstClick = true;
                                        (*it)->lastClickTime = now;
                                    }
                                }
                                else {
                                    (*it)->firstClick = true;
                                    (*it)->lastClickTime = now;
                                }

                                // Dacă nu este rotație, începe drag-ul
                                if (!(*it)->isRotating) {
                                    (*it)->startDragging(mousePosF);
                                }

                                // Mută elementul la final
                                auto draggable = *it;
                                draggableSquares.erase(std::next(it).base());
                                draggableSquares.push_back(draggable);
                                break;
                            }
                        }

                        // 3. După loop, ascunde hitbox-urile dacă nu s-a dat click pe niciun DraggableSquare
                        if (!clickedOnAnyDraggable) {
                            for (auto& d : draggableSquares) {
                                if (!isValoriMenuOpen)
                                    d->hitboxVisible = false;
                            }
                        }
                        if (isValoriMenuOpen) {
                            // Check if the close button (X) was clicked
                            if (valoriCloseButton.getGlobalBounds().contains(uiMousePos)) {
                                isValoriMenuOpen = false;
                            }

                            // Check if any input box was clicked
                            for (size_t i = 0; i < valoriInputBoxes.size(); ++i) {
                                if (valoriInputBoxes[i].getGlobalBounds().contains(uiMousePos)) {
                                    selectedInputBox = static_cast<int>(i);
                                    break;
                                }
                            }
                        }
                    }
                    else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
                        for (auto& draggable : draggableSquares) {
                            draggable->stopDragging();
                            // Oprește rotația
                            for (auto& draggable : draggableSquares) {
                                if (draggable->isRotating) {
                                    draggable->isRotating = false;
                                }
                            }
                            // Verificăm dacă poziția este sub header
                            if (draggable->position.y < (static_cast<float>(screenHeight) / 8.f)) {
                                // Dacă componenta este plasată pe header, eliminăm componenta
                                auto it = std::find_if(draggableSquares.begin(), draggableSquares.end(),
                                    [&](const std::shared_ptr<DraggableSquare>& ds) { return ds->position == draggable->position; });

                                if (it != draggableSquares.end()) {
                                    draggableSquares.erase(it);
                                }
                            }
                        }
                    }
                    else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Right) {
                        for (auto& draggable : draggableSquares) {
                            if (draggable->isRotating) {
                                draggable->isRotating = false;
                            }
                        }
                    }
                    // Gestionare Click Dreapta pentru Conexiuni
                    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Right) {
                        // Conversia poziției mouse-ului în coordonate ale lumii și de ecran
                        sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
                        sf::Vector2f mousePosF = window.mapPixelToCoords(pixelPos, view);
                        sf::Vector2f uiMousePos = window.mapPixelToCoords(pixelPos, window.getDefaultView());

                        bool clickedOnPort = false;

                        // Verificăm dacă s-a făcut clic dreapta pe un port al unei componente mobile
                        for (auto& comp : draggableSquares) {
                            for (int portIndex = 0; portIndex < comp->pieceDrawer->getLinks().size(); ++portIndex) {
                                sf::Vector2f portPos = comp->getPortPosition(portIndex);
                                float portRadius = 10.f * comp->localScale; // Raza portului pentru detectarea clicului
                                sf::CircleShape portShape(portRadius);
                                portShape.setPosition(portPos.x - portRadius, portPos.y - portRadius); // Centrat pe port
                                float dx = mousePosF.x - portPos.x;
                                float dy = mousePosF.y - portPos.y;
                                if ((dx * dx + dy * dy) <= (portRadius * portRadius)) {
                                    clickedOnPort = true;
                                    if (!isConnecting) {
                                        // Verificăm dacă portul sursă este deja conectat
                                        if (isPortConnected(connections, comp, portIndex)) {
                                            // Portul este deja conectat, nu putem începe o nouă conexiune
                                            std::cout << "Portul selectat este deja conectat." << std::endl;
                                            break;
                                        }

                                        // Începem o conexiune
                                        isConnecting = true;
                                        connectionSource = comp;
                                        connectionSourcePort = portIndex;

                                        // Inițializăm linia temporară
                                        tempLine[0].position = portPos;
                                        tempLine[0].color = sf::Color::Yellow;
                                        tempLine[1].position = mousePosF;
                                        tempLine[1].color = sf::Color::Yellow;
                                    }
                                    else {
                                        // Finalizăm conexiunea
                                        // Verificăm dacă portul țintă este deja conectat
                                        if (isPortConnected(connections, comp, portIndex) || (connectionSource == comp && connectionSourcePort == portIndex)) {
                                            // Port deja conectat sau același port, anulăm conexiunea
                                            isConnecting = false;
                                            connectionSource = nullptr;
                                            connectionSourcePort = -1;
                                            break;
                                        }

                                        // Creăm o nouă conexiune
                                        // Determină direcția inițială a cotiturilor
                                        sf::Vector2f srcPos = connectionSource->getPortPosition(connectionSourcePort);
                                        sf::Vector2f tgtPos = comp->getPortPosition(portIndex);

                                        // Calculăm distanța verticală între porturi
                                        float verticalDistance = std::abs(srcPos.y - tgtPos.y);

                                        // Determinăm direcția inițială în funcție de poziție și distanță
                                        bool initialDirectionUp = (tgtPos.y < srcPos.y) && (verticalDistance < distanceThreshold);

                                        // Creăm o nouă conexiune cu direcția inițială
                                        connections.emplace_back(connectionSource, connectionSourcePort, comp, portIndex, initialDirectionUp);
                                        isConnecting = false;
                                        connectionSource = nullptr;
                                        connectionSourcePort = -1;
                                    }
                                    break; // O singură conexiune per clic
                                }
                            }
                            if (clickedOnPort) break;
                        }
                        // Detectarea clicului dreapta pe cercul de rotație pentru rotație
                        for (auto& draggable : draggableSquares) {
                            if (draggable->hitboxVisible) {
                                // Definește cercul albastru
                                sf::CircleShape rotateCircleLocal(8.f);
                                rotateCircleLocal.setFillColor(sf::Color::Blue);
                                rotateCircleLocal.setOrigin(rotateCircleLocal.getRadius(), rotateCircleLocal.getRadius());

                                // Define offset-ul pentru cercul de rotație relativ la centrul componentei
                                float extraPadding = 0.f; // Ajustează dacă este necesar
                                sf::Vector2f rotateCircleOffset((hitboxSize * draggable->localScale) / 2.f - rotateCircleLocal.getRadius() - extraPadding,
                                    -(hitboxSize * draggable->localScale) / 2.f + rotateCircleLocal.getRadius() + extraPadding);

                                // Rotim offset-ul în funcție de unghiul de rotație
                                sf::Vector2f rotatedOffset = draggable->rotatePoint(rotateCircleOffset, sf::Vector2f(0.f, 0.f), draggable->rotationAngle);

                                // Calculăm poziția cercului de rotație
                                sf::Vector2f rotateCirclePos = draggable->position + rotatedOffset;
                                rotateCircleLocal.setPosition(rotateCirclePos);

                                // Calculăm distanța între mouse și cercul de rotație
                                float dx_rot = mousePosF.x - rotateCirclePos.x;
                                float dy_rot = mousePosF.y - rotateCirclePos.y;
                                if ((dx_rot * dx_rot + dy_rot * dy_rot) <= (rotateCircleLocal.getRadius() * rotateCircleLocal.getRadius())) {
                                    draggable->isRotating = true;
                                    draggable->initialAngle = draggable->rotationAngle;
                                    draggable->rotationStartMousePos = mousePosF;
                                    break;
                                }
                            }
                        }

                        // Dacă s-a făcut clic dreapta pe un port al unei componente statice (dacă există)
                        // Repetă același proces pentru StaticSquare dacă este necesar

                        if (!clickedOnPort && isConnecting) {
                            // Clic dreapta oriunde altundeva pentru a anula conexiunea
                            isConnecting = false;
                            connectionSource = nullptr;
                            connectionSourcePort = -1;
                        }
                    }
                    if (isSaveAsPopupVisible && event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                        sf::Vector2f mousePosF = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                        if (saveAsCloseButton.getGlobalBounds().contains(mousePosF)) {
                            isSaveAsPopupVisible = false;
                        }
                    }
                    if (event.type == sf::Event::MouseMoved) {
                        sf::Vector2f mousePosF = window.mapPixelToCoords(sf::Vector2i(event.mouseMove.x, event.mouseMove.y), view);
                        for (auto& d : draggableSquares) {
                            if (d->isRotating) {
                                // Calculăm vectorul de la centru la current mouse position
                                sf::Vector2f center = d->position;
                                sf::Vector2f delta = mousePosF - center;

                                // Calculăm unghiul în funcție de direcția mouse-ului
                                int angle = atan2(delta.y, delta.x) * 180.f / 3.14159f;

                                // Actualizăm unghiul de rotație
                                if (angle % 45 == 0)
                                    d->rotationAngle = angle;
                            }
                            else if (d->isBeingDragged) {
                                // Actualizează poziția doar dacă componenta este în drag
                                d->updateDrag(mousePosF);
                            }
                        }
                    }
                }
                if (event.type == sf::Event::TextEntered && isValoriMenuOpen && selectedInputBox >= 0 && ok) {
                    ok = false;
                    if (event.text.unicode == 8) { // Backspace
                        auto& label = valoriLabels[selectedInputBox];
                        std::string currentStr = label.getString();
                        if (!currentStr.empty()) {
                            currentStr.pop_back();
                            label.setString(currentStr);
                        }
                    }
                    else if (isdigit(event.text.unicode) && valoriLabels[selectedInputBox].getString().getSize() < 5) {
                        auto& label = valoriLabels[selectedInputBox];
                        std::string currentStr = label.getString();
                        currentStr += static_cast<char>(event.text.unicode);
                        label.setString(currentStr);
                    }
                    // Actualizare valoare în componentValues
                    for (auto& draggable : draggableSquares) {
                        if (draggable->hitboxVisible) {
                            for (int i = 0;i < draggable->componentValues.size();i++) {
                                sf::String sfmlString = valoriLabels[i].getString();
                                std::string standardString = sfmlString.toAnsiString();
                                draggable->componentValues[i] = standardString;
                            }

                        }
                    }
                }
                if (event.type == sf::Event::KeyReleased)
                    ok = true;
                // Actualizare Linie Temporară în timpul conexiunii
                if (isConnecting) {
                    sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
                    sf::Vector2f mousePosF = window.mapPixelToCoords(pixelPos, view);
                    tempLine[1].position = mousePosF;
                }
                // Gestionare tastă '+'
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Add)) {
                    for (auto& d : draggableSquares) {
                        if (d->hitboxVisible) {
                            d->localScale += 0.1f;
                        }
                    }
                }

                // Gestionare tastă '-'
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Subtract)) {
                    for (auto& d : draggableSquares) {
                        if (d->hitboxVisible) {
                            d->localScale -= 0.1f;
                        }
                    }
                }
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
                    view.move(-cameraSpeed, 0.f); // Muta camera la stânga
                }

                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
                    view.move(cameraSpeed, 0.f); // Muta camera la dreapta
                }

                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
                    view.move(0.f, -cameraSpeed); // Muta camera în sus
                }

                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
                    view.move(0.f, cameraSpeed); // Muta camera în jos
                }
                if (isSaveAsPopupVisible && event.type == sf::Event::TextEntered) {
                    if (event.text.unicode == '\b') { // Backspace
                        if (!saveAsTextBoxInput.empty()) {
                            saveAsTextBoxInput.pop_back();
                            saveAsTextBoxText.setString(saveAsTextBoxInput);
                        }
                    }
                    else if (event.text.unicode == '\r') { // Enter
                        if (!saveAsTextBoxInput.empty()) {
                            // TODO: Ce se întâmplă când se apasă Enter
                            saveAsFileName = saveAsTextBoxInput + ".elect";
                            std::ofstream outFile(saveAsFileName, std::ios::out | std::ios::trunc);
                            outFile << draggableSquares.size() << '\n';
                            for (int i = 0;i < draggableSquares.size();i++) {
                                outFile << draggableSquares[i]->id << ' ' << draggableSquares[i]->position.x << ' ' << draggableSquares[i]->position.y << ' ' << draggableSquares[i]->pieceDrawer->fileNameForDrawer << ' ' << draggableSquares[i]->localScale << ' ' << draggableSquares[i]->rotationAngle << ' ';
                                for (int j = 0;j < draggableSquares[i]->componentValues.size();j++)
                                    outFile << draggableSquares[i]->componentValues[j] << ' ';
                                outFile << '\n';
                            }
                            outFile << connections.size() << '\n';
                            for (int i = 0;i < connections.size();i++)
                                outFile << connections[i].sourceComponent->id << ' ' << connections[i].sourcePortIndex << ' ' << connections[i].targetComponent->id << ' ' << connections[i].targetPortIndex << '\n';
                            outFile << idAct;
                            outFile.close();
                            isSaveAsPopupVisible = false;
                        }
                    }
                    else if (event.text.unicode < 128 && okSaveAs) {
                        okSaveAs = false;
                        saveAsTextBoxInput += static_cast<char>(event.text.unicode);
                        saveAsTextBoxText.setString(saveAsTextBoxInput);

                        // Actualizează poziția cursorului
                        sf::FloatRect textBounds = saveAsTextBoxText.getGlobalBounds();
                        saveAsCursor.setPosition(textBounds.left + textBounds.width + 5.f, saveAsCursor.getPosition().y);
                    }
                }
                if (event.type == sf::Event::KeyReleased) {
                    okSaveAs = true;
                }

                // Restricționăm centrul view-ului astfel încât să nu depășească limitele foii metrice
                sf::Vector2f viewCenter = view.getCenter();
                sf::Vector2f viewSize = view.getSize();

                // Calculăm limitele view-ului
                float leftLimit = sheetBounds.left + viewSize.x / 2;
                float rightLimit = sheetBounds.left + sheetBounds.width - viewSize.x / 2;
                float topLimit = sheetBounds.top + viewSize.y / 2;
                float bottomLimit = sheetBounds.top + sheetBounds.height - viewSize.y / 2;

                // Ajustăm centrul view-ului dacă este necesar
                if (viewCenter.x < leftLimit) viewCenter.x = leftLimit;
                if (viewCenter.x > rightLimit) viewCenter.x = rightLimit;
                if (viewCenter.y < topLimit) viewCenter.y = topLimit;
                if (viewCenter.y > bottomLimit) viewCenter.y = bottomLimit;

                // Setăm noul centru al view-ului
                view.setCenter(viewCenter);

                if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                    sf::Vector2f worldMousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), view);
                    for (auto& draggable : draggableSquares) {
                        draggable->updateDrag(worldMousePos);
                    }
                }

                // Curățăm fereastra
                window.clear(sf::Color::Black);

                window.setView(view);
                drawGrid(window, view, gridSpacing, gridColor); // Grid-ul

                // Desenăm DraggableSquares înainte de header pentru a fi dedesubt
                for (auto& draggable : draggableSquares) {
                    if (draggable->position.y > header.getSize().y)
                        draggable->draw(window);
                }

                // Desenăm conexiunile conform regulilor specificate
                for (const auto& conn : connections) {
                    // Obținem pozițiile porturilor conectate
                    sf::Vector2f srcPos = conn.sourceComponent->getPortPosition(conn.sourcePortIndex);
                    sf::Vector2f tgtPos = conn.targetComponent->getPortPosition(conn.targetPortIndex);

                    if (conn.sourceComponent == conn.targetComponent) {
                        // Păstrează logica existentă pentru conexiunile în cadrul aceleiași componente
                        float padding = 10.f; // Ajustează padding-ul după necesități

                        float hitboxHalfSize = (hitboxSize * conn.sourceComponent->localScale) / 2.f + padding;

                        // Calculăm distanța verticală între porturi
                        float verticalDistance = std::abs(srcPos.y - tgtPos.y);

                        // Determinăm direcția conexiunii în funcție de distanță
                        bool goUp = (tgtPos.y < srcPos.y);

                        sf::Vector2f p1 = srcPos;
                        if (goUp) {
                            p1.y -= hitboxHalfSize;
                        }
                        else {
                            p1.y += hitboxHalfSize;
                        }

                        sf::Vector2f p2 = sf::Vector2f(tgtPos.x, p1.y);

                        sf::VertexArray lines(sf::Lines, 6);

                        // Linia 1: srcPos -> p1
                        lines[0].position = srcPos;
                        lines[0].color = sf::Color::Yellow;
                        lines[1].position = p1;
                        lines[1].color = sf::Color::Yellow;

                        // Linia 2: p1 -> p2
                        lines[2].position = p1;
                        lines[2].color = sf::Color::Yellow;
                        lines[3].position = p2;
                        lines[3].color = sf::Color::Yellow;

                        // Linia 3: p2 -> tgtPos
                        lines[4].position = p2;
                        lines[4].color = sf::Color::Yellow;
                        lines[5].position = tgtPos;
                        lines[5].color = sf::Color::Yellow;

                        // Desenăm conexiunea
                        window.draw(lines);
                    }
                    else {
                        // Conexiune între componente diferite

                        // Obținem pozițiile porturilor conectate
                        sf::Vector2f srcPos = conn.sourceComponent->getPortPosition(conn.sourcePortIndex);
                        sf::Vector2f tgtPos = conn.targetComponent->getPortPosition(conn.targetPortIndex);

                        // Colectăm obstacolele (hitbox-urile), excluzând componentele sursă și țintă
                        std::vector<sf::FloatRect> obstacles;
                        for (const auto& d : draggableSquares) {
                            if (d.get() != conn.sourceComponent.get() && d.get() != conn.targetComponent.get()) {
                                obstacles.push_back(d->hitbox.getGlobalBounds());
                            }
                        }

                        // Determinăm dacă componentele sunt pe aceeași linie orizontală
                        bool sameHorizontalLine = std::abs(srcPos.y - tgtPos.y) < 5.f; // Prag ajustabil

                        if (sameHorizontalLine) {
                            // 1. Conexiune pe aceeași linie orizontală
                            bool straightClear = true;
                            for (const auto& rect : obstacles) {
                                if (lineIntersectsRect(srcPos, tgtPos, rect)) {
                                    straightClear = false;
                                    break;
                                }
                            }

                            if (straightClear) {
                                // Desenează linia dreaptă
                                sf::Vertex line[] = {
                                    sf::Vertex(srcPos, sf::Color::Yellow),
                                    sf::Vertex(tgtPos, sf::Color::Yellow)
                                };
                                window.draw(line, 2, sf::Lines);
                            }
                            else {
                                // Dacă linia dreaptă este blocată, încearcă să desenezi o conexiune cu trei linii și două unghiuri de 90°
                                std::vector<sf::Vector2f> threeLinePath = computeThreeLinePath(srcPos, tgtPos, obstacles, true); // preferDirectionUp = true

                                bool canDrawThreeLine = !threeLinePath.empty();

                                if (canDrawThreeLine) {
                                    // Verifică dacă traseul este clar
                                    bool pathClear = true;
                                    for (size_t i = 0; i < threeLinePath.size() - 1; ++i) {
                                        for (const auto& rect : obstacles) {
                                            if (lineIntersectsRect(threeLinePath[i], threeLinePath[i + 1], rect)) {
                                                pathClear = false;
                                                break;
                                            }
                                        }
                                        if (!pathClear)
                                            break;
                                    }

                                    if (pathClear) {
                                        // Desenează traseul cu trei linii și două unghiuri de 90°
                                        sf::VertexArray lines(sf::Lines, (threeLinePath.size() - 1) * 2);
                                        for (size_t i = 0; i < threeLinePath.size() - 1; ++i) {
                                            lines[i * 2].position = threeLinePath[i];
                                            lines[i * 2].color = sf::Color::Yellow;
                                            lines[i * 2 + 1].position = threeLinePath[i + 1];
                                            lines[i * 2 + 1].color = sf::Color::Yellow;
                                        }
                                        window.draw(lines);
                                    }
                                    else {
                                        // Dacă traseul nu este clar, șterge obstacolul care blochează conexiunea
                                        for (const auto& rect : obstacles) {
                                            if (lineIntersectsRect(srcPos, tgtPos, rect)) {
                                                // Găsește DraggableSquare care conține acest rect
                                                for (auto it = draggableSquares.begin(); it != draggableSquares.end(); ++it) {
                                                    sf::FloatRect currentRect = (*it)->hitbox.getGlobalBounds();
                                                    if (currentRect == rect) { // Comparare directă a dreptunghiurilor
                                                        // Șterge obstacle
                                                        draggableSquares.erase(it);
                                                        std::cout << "Obstacle deleted due to blocked straight connection." << std::endl;
                                                        break; // Ieșim din bucla interioară după ștergere
                                                    }
                                                }
                                                break; // Ieșim din bucla exterioară după găsirea și ștergerea obstacolului
                                            }
                                        }
                                        // Conexiunea va fi reîncercată în următorul frame, acum fără obstacolul șters
                                    }
                                }
                                else {
                                    // Dacă nu se poate desena conexiunea cu trei linii, șterge obstacolul și reîncearcă
                                    for (const auto& rect : obstacles) {
                                        if (lineIntersectsRect(srcPos, tgtPos, rect)) {
                                            // Găsește DraggableSquare care conține acest rect
                                            for (auto it = draggableSquares.begin(); it != draggableSquares.end(); ++it) {
                                                sf::FloatRect currentRect = (*it)->hitbox.getGlobalBounds();
                                                if (currentRect == rect) { // Comparare directă a dreptunghiurilor
                                                    // Șterge obstacle
                                                    draggableSquares.erase(it);
                                                    std::cout << "Obstacle deleted due to blocked straight connection." << std::endl;
                                                    break; // Ieșim din bucla interioară după ștergere
                                                }
                                            }
                                            break; // Ieșim din bucla exterioară după găsirea și ștergerea obstacolului
                                        }
                                    }
                                }
                            }
                        }
                        else {
                            // 2. Conexiune în diagonală

                            // a. Opțiunea 1: Horizontal -> Vertical
                            sf::Vector2f p1_h(srcPos.x, tgtPos.y);
                            bool path1Clear = true;
                            for (const auto& rect : obstacles) {
                                if (lineIntersectsRect(srcPos, p1_h, rect) || lineIntersectsRect(p1_h, tgtPos, rect)) {
                                    path1Clear = false;
                                    break;
                                }
                            }

                            if (path1Clear) {
                                // Desenează conexiunea cu două linii și un unghi de 90°
                                sf::VertexArray lines(sf::Lines, 4);
                                lines[0].position = srcPos;
                                lines[0].color = sf::Color::Yellow;
                                lines[1].position = p1_h;
                                lines[1].color = sf::Color::Yellow;

                                lines[2].position = p1_h;
                                lines[2].color = sf::Color::Yellow;
                                lines[3].position = tgtPos;
                                lines[3].color = sf::Color::Yellow;

                                window.draw(lines);
                            }
                            else {
                                // b. Opțiunea 2: Vertical -> Horizontal
                                sf::Vector2f p1_v(tgtPos.x, srcPos.y);
                                bool path2Clear = true;
                                for (const auto& rect : obstacles) {
                                    if (lineIntersectsRect(srcPos, p1_v, rect) || lineIntersectsRect(p1_v, tgtPos, rect)) {
                                        path2Clear = false;
                                        break;
                                    }
                                }

                                if (path2Clear) {
                                    // Desenează conexiunea cu două linii și un unghi de 90° (al doilea sens)
                                    sf::VertexArray lines(sf::Lines, 4);
                                    lines[0].position = srcPos;
                                    lines[0].color = sf::Color::Yellow;
                                    lines[1].position = p1_v;
                                    lines[1].color = sf::Color::Yellow;

                                    lines[2].position = p1_v;
                                    lines[2].color = sf::Color::Yellow;
                                    lines[3].position = tgtPos;
                                    lines[3].color = sf::Color::Yellow;

                                    window.draw(lines);
                                }
                                else {
                                    // c. Dacă ambele opțiuni de două linii sunt blocate, încearcă să desenezi o conexiune cu trei linii și două unghiuri de 90°
                                    std::vector<sf::Vector2f> threeLinePath = computeThreeLinePath(srcPos, tgtPos, obstacles, true); // preferDirectionUp = true

                                    bool canDrawThreeLine = !threeLinePath.empty();

                                    if (canDrawThreeLine) {
                                        // Verifică dacă traseul este clar
                                        bool pathClear = true;
                                        for (size_t i = 0; i < threeLinePath.size() - 1; ++i) {
                                            for (const auto& rect : obstacles) {
                                                if (lineIntersectsRect(threeLinePath[i], threeLinePath[i + 1], rect)) {
                                                    pathClear = false;
                                                    break;
                                                }
                                            }
                                            if (!pathClear)
                                                break;
                                        }

                                        if (pathClear) {
                                            // Desenează conexiunea cu trei linii și două unghiuri de 90°
                                            sf::VertexArray lines(sf::Lines, (threeLinePath.size() - 1) * 2);
                                            for (size_t i = 0; i < threeLinePath.size() - 1; ++i) {
                                                lines[i * 2].position = threeLinePath[i];
                                                lines[i * 2].color = sf::Color::Yellow;
                                                lines[i * 2 + 1].position = threeLinePath[i + 1];
                                                lines[i * 2 + 1].color = sf::Color::Yellow;
                                            }
                                            window.draw(lines);
                                        }
                                        else {
                                            // Dacă traseul nu este clar, șterge obstacolul care blochează conexiunea
                                            for (const auto& rect : obstacles) {
                                                if (lineIntersectsRect(srcPos, tgtPos, rect)) {
                                                    // Găsește DraggableSquare care conține acest rect
                                                    for (auto it = draggableSquares.begin(); it != draggableSquares.end(); ++it) {
                                                        sf::FloatRect currentRect = (*it)->hitbox.getGlobalBounds();
                                                        if (currentRect == rect) { // Comparare directă a dreptunghiurilor
                                                            // Șterge obstacle
                                                            draggableSquares.erase(it);
                                                            std::cout << "Obstacle deleted due to blocked three-line connection." << std::endl;
                                                            break; // Ieșim din bucla interioară după ștergere
                                                        }
                                                    }
                                                    break; // Ieșim din bucla exterioară după găsirea și ștergerea obstacolului
                                                }
                                            }
                                            // Conexiunea va fi reîncercată în următorul frame, acum fără obstacolul șters
                                        }
                                    }
                                    else {
                                        // Dacă nu se poate desena conexiunea cu trei linii, șterge obstacolul și reîncearcă
                                        for (const auto& rect : obstacles) {
                                            if (lineIntersectsRect(srcPos, tgtPos, rect)) {
                                                // Găsește DraggableSquare care conține acest rect
                                                for (auto it = draggableSquares.begin(); it != draggableSquares.end(); ++it) {
                                                    sf::FloatRect currentRect = (*it)->hitbox.getGlobalBounds();
                                                    if (currentRect == rect) { // Comparare directă a dreptunghiurilor
                                                        // Șterge obstacle
                                                        draggableSquares.erase(it);
                                                        std::cout << "Obstacle deleted due to blocked three-line connection." << std::endl;
                                                        break; // Ieșim din bucla interioară după ștergere
                                                    }
                                                }
                                                break; // Ieșim din bucla exterioară după găsirea și ștergerea obstacolului
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        // Nou: Verificare dacă linia trece prin centrul unei componente
            // Obține centrul componentelor
                        sf::Vector2f sourceCenter = conn.sourceComponent->position;
                        sf::Vector2f targetCenter = conn.targetComponent->position;

                        // Calculăm distanțele de la centre la linia conexiunii
                        float distanceSource = pointToLineDistance(sourceCenter, srcPos, tgtPos);
                        float distanceTarget = pointToLineDistance(targetCenter, srcPos, tgtPos);

                        // Definim pragul de distanță pentru a considera dacă linia trece prin centru
                        const float epsilon = 5.f;

                        bool passesThroughCenter = (distanceSource < epsilon) || (distanceTarget < epsilon);

                        if (passesThroughCenter) {
                            // Conexiunea trece prin centru, recalculăm traseul

                            // Recalculăm traseul folosind computeThreeLinePath
                            std::vector<sf::Vector2f> newPath = computeThreeLinePath(srcPos, tgtPos, obstacles, true); // preferDirectionUp = true

                            // Desenăm traseul nou
                            for (size_t i = 0; i < newPath.size() - 1; ++i) {
                                sf::Vertex line[] = {
                                    sf::Vertex(newPath[i], sf::Color::Yellow),
                                    sf::Vertex(newPath[i + 1], sf::Color::Yellow)
                                };
                                window.draw(line, 2, sf::Lines);
                            }
                        }
                        else {
                            // Conexiunea nu trece prin centru, desenează conexiunea normală

                            // (Reintroduce blocul existent de desenare al conexiunilor aici)
                            // Acest cod este deja inclus în blocul else, deci nu este necesară repetarea
                        }

                    }
                }

                // Desenăm linia temporară dacă este în curs de conexiune
                if (isConnecting) {
                    window.draw(tempLine);
                }

                // Set view-ul la default pentru UI
                window.setView(window.getDefaultView());

                // Desenăm header-ul
                window.draw(header);

                // Desenăm butonul "Delete"
                window.draw(deleteButton);
                window.draw(deleteButtonText);

                // Desenăm butonul (cele trei linii)
                if (!isMenuOpen) {
                    window.draw(line1);
                    window.draw(line2);
                    window.draw(line3);
                }

                if (isValoriMenuOpen) {
                    window.draw(valoriMenu);
                    window.draw(valoriTitle);
                    window.draw(valoriCloseButton);

                    for (size_t i = 0; i < valoriLabels.size(); ++i) {
                        window.draw(valoriInputBoxes[i]);
                        window.draw(valoriLabels[i]);
                        window.draw(valoriUnits[i]);
                        window.draw(dimensions[i]);
                    }
                }

                // Desenăm cele cinci pătrate din header
                window.draw(square1);
                window.draw(square2);
                window.draw(square3);
                window.draw(square4);
                window.draw(square5);

                // Desenăm staticSquares (pătratele 2, 3 și 4)
                for (auto& staticSquare : staticSquares) {
                    staticSquare.draw(window);
                }

                // Desenăm meniul dacă este deschis
                if (isMenuOpen) {
                    window.draw(menu);
                    window.draw(backButton);
                    window.draw(saveButton);
                    window.draw(saveAsButton);
                    window.draw(resetButton);
                    window.draw(backText);
                    window.draw(saveText);
                    window.draw(saveAsText);
                    window.draw(resetText);
                    window.draw(closeButton);
                    window.draw(xLines); // Desenăm X-ul de închidere
                }

                if (isSaveAsPopupVisible) {
                    window.draw(saveAsPopup);
                    window.draw(saveAsTitle);
                    window.draw(saveAsTextBoxText);
                    window.draw(saveAsCloseButton);
                    if (isCursorVisible) {
                        window.draw(saveAsCursor);
                    }
                }

                // Desenăm popup-ul dacă este afișat
                if (showPopup) {
                    window.setView(window.getDefaultView());
                    float popupWidth = 400.f;
                    float popupHeight = 150.f;
                    popup.setSize(sf::Vector2f(popupWidth, popupHeight));
                    popup.setFillColor(sf::Color(169, 169, 169)); // Gri
                    popup.setPosition((screenWidth - popupWidth) / 2, (screenHeight - popupHeight) / 2);

                    popupText.setFont(font);
                    popupText.setString(popupMessage);
                    popupText.setCharacterSize(20);
                    popupText.setFillColor(sf::Color::Black);
                    popupText.setPosition(
                        popup.getPosition().x + (popupWidth - popupText.getLocalBounds().width) / 2,
                        popup.getPosition().y + (popupHeight - popupText.getLocalBounds().height) / 2
                    );

                    float closeXSize = 30.f;
                    sf::RectangleShape closeX(sf::Vector2f(closeXSize, closeXSize));
                    closeX.setFillColor(sf::Color::Red);
                    closeX.setPosition(popup.getPosition().x + popupWidth - closeXSize - 10.f, popup.getPosition().y + 10.f);
                    // Setăm poziția butonului "X" în popup
                    popupCloseButton.setPosition(popup.getPosition().x + popupWidth - 30.f - 10.f, // 30.f este dimensiunea butonului
                        popup.getPosition().y + 10.f);

                    sf::VertexArray xClose(sf::Lines, 4);
                    xClose[0].position = sf::Vector2f(closeX.getPosition().x + 5.f, closeX.getPosition().y + 5.f);
                    xClose[0].color = sf::Color::White;
                    xClose[1].position = sf::Vector2f(closeX.getPosition().x + closeXSize - 5.f, closeX.getPosition().y + closeXSize - 5.f);
                    xClose[1].color = sf::Color::White;

                    xClose[2].position = sf::Vector2f(closeX.getPosition().x + closeXSize - 5.f, closeX.getPosition().y + 5.f);
                    xClose[2].color = sf::Color::White;
                    xClose[3].position = sf::Vector2f(closeX.getPosition().x + 5.f, closeX.getPosition().y + closeXSize - 5.f);
                    xClose[3].color = sf::Color::White;

                    window.draw(popup);
                    window.draw(popupText);
                    window.draw(closeX);
                    window.draw(xClose);
                    if (popupClock.getElapsedTime().asSeconds() > 10.f) {
                        showPopup = false;
                    }
                    // Verificăm dacă X-ul este apăsat
                    // Verificăm dacă X-ul este apăsat
                    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                        // Conversia poziției mouse-ului în coordonate ale lumii pentru UI
                        // Conversia poziției mouse-ului în coordonate ale lumii pentru UI
                        sf::Vector2f mousePosF = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y), window.getDefaultView());
                        if (popupCloseButton.getGlobalBounds().contains(mousePosF)) {
                            showPopup = false; // Închidem popup-ul dacă s-a apăsat X-ul
                        }
                    }
                    window.draw(sheetBorder); // Desenăm limitele

                }
                window.display();
            }

        }
    }
}

// -------------------------------------------
//     Window / Font / Title Screen Setup
// -------------------------------------------
void initializeWindow(sf::RenderWindow& window) {
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);
}

sf::Font loadSansSerifFont() {
    sf::Font font;
#ifdef _WIN32
    if (!font.loadFromFile("C:\\Windows\\Fonts\\arial.ttf")) {
        std::cerr << "Windows font loading failed" << std::endl;
#elif __APPLE__
    if (!font.loadFromFile("/System/Library/Fonts/Helvetica.ttc")) {
        std::cerr << "macOS font loading failed" << std::endl;
#else
    if (!font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
        std::cerr << "Linux font loading failed" << std::endl;
#endif
        if (!font.loadFromFile("default_sans_serif.ttf")) {
            std::cerr << "Critical: No fonts available" << std::endl;
            exit(1);
        }
    }
    return font;
    }

void drawTitleScreen(sf::RenderWindow & window, sf::Font & font) {
    sf::Text titleText;
    titleText.setFont(font);
    titleText.setString("ELECTRON");
    titleText.setCharacterSize(250);
    titleText.setFillColor(sf::Color::White);

    sf::FloatRect textBounds = titleText.getLocalBounds();
    titleText.setOrigin(textBounds.left + textBounds.width / 2.0f,
        textBounds.top + textBounds.height / 2.0f);
    titleText.setPosition(sf::Vector2f(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f));

    for (float opacity = 255.0f; opacity > 0; opacity -= 5.0f) {
        window.clear(sf::Color(28, 28, 28));
        titleText.setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(opacity)));
        window.draw(titleText);
        window.display();
        sf::sleep(sf::milliseconds(35));
    }
}

// -------------------------------------------
//        Button Creation and Colors
// -------------------------------------------
void createMainMenuButtons(Button * buttons, int& buttonCount, sf::Font & font) {
    const char* buttonLabels[] = {
        "New Project", "Open Project", "Information", "Settings", "Exit"
    };

    buttonCount = 5;
    float titleYPosition = SCREEN_HEIGHT * 0.25f;
    float buttonStartY = titleYPosition + 200;  // padding

    for (int i = 0; i < buttonCount; ++i) {
        buttons[i].shape.setSize(sf::Vector2f(300, 60));
        buttons[i].shape.setFillColor(sf::Color(100, 100, 100));
        buttons[i].shape.setOutlineColor(sf::Color(50, 50, 50));
        buttons[i].shape.setOutlineThickness(3);

        buttons[i].shape.setOrigin(150, 30);
        buttons[i].shape.setPosition(SCREEN_WIDTH / 2.0f, buttonStartY + i * 100);

        buttons[i].text.setFont(font);
        buttons[i].text.setString(buttonLabels[i]);
        buttons[i].text.setCharacterSize(30);
        buttons[i].text.setFillColor(sf::Color::White);

        sf::FloatRect textBounds = buttons[i].text.getLocalBounds();
        buttons[i].text.setOrigin(textBounds.left + textBounds.width / 2.0f, textBounds.top + textBounds.height / 2.0f);

        // Adjust position to move text higher by 10 pixels
        buttons[i].text.setPosition(buttons[i].shape.getPosition().x, buttons[i].shape.getPosition().y);

        buttons[i].isHovered = false;
    }
}

void createSettingsButtons(Button * buttons, int& buttonCount, sf::Font & font) {
    const char* buttonLabels[] = {
        "New Component", "HotKeys", "Back"
    };

    buttonCount = 3;
    float titleYPosition = SCREEN_HEIGHT * 0.25f;
    float buttonStartY = titleYPosition + 200; // padding

    for (int i = 0; i < buttonCount; ++i) {
        buttons[i].shape.setSize(sf::Vector2f(300, 60));
        buttons[i].shape.setFillColor(sf::Color(100, 100, 100));
        buttons[i].shape.setOutlineColor(sf::Color(50, 50, 50));
        buttons[i].shape.setOutlineThickness(3);

        buttons[i].shape.setOrigin(150, 30);
        buttons[i].shape.setPosition(SCREEN_WIDTH / 2.0f, buttonStartY + i * 100);

        buttons[i].text.setFont(font);
        buttons[i].text.setString(buttonLabels[i]);
        buttons[i].text.setCharacterSize(30);
        buttons[i].text.setFillColor(sf::Color::White);

        sf::FloatRect textBounds = buttons[i].text.getLocalBounds();
        buttons[i].text.setOrigin(textBounds.left + textBounds.width / 2.0f,
            buttons[i].text.getOrigin().y);
        buttons[i].text.setPosition(buttons[i].shape.getPosition().x, buttons[i].shape.getPosition().y - 20);

        buttons[i].isHovered = false;
    }
}

void createInformationButtons(Button * buttons, int& buttonCount, sf::Font & font) {
    buttonCount = 1; // Only "Back"

    buttons[0].shape.setSize(sf::Vector2f(200, 50));
    buttons[0].shape.setFillColor(sf::Color(100, 100, 100));
    buttons[0].shape.setOutlineColor(sf::Color(50, 50, 50));
    buttons[0].shape.setOutlineThickness(3);
    buttons[0].shape.setPosition(SCREEN_WIDTH / 2.0f - 100, SCREEN_HEIGHT - 100);

    buttons[0].text.setFont(font);
    buttons[0].text.setString("Back");
    buttons[0].text.setCharacterSize(20);
    buttons[0].text.setFillColor(sf::Color::White);

    sf::FloatRect textBounds = buttons[0].text.getLocalBounds();
    buttons[0].text.setOrigin(textBounds.left + textBounds.width / 2.0f,
        buttons[0].text.getOrigin().y);
    buttons[0].text.setPosition(buttons[0].shape.getPosition().x + 100,
        buttons[0].shape.getPosition().y + 20);

    buttons[0].isHovered = false;
}

void createNewComponentButtons(Button * buttons, int& buttonCount, sf::Font & font) {
    const char* buttonLabels[] = { "Cancel", "Refresh", "Create" };
    buttonCount = 3;

    float buttonWidth = 200;
    float spacing = (SCREEN_WIDTH - (buttonWidth * 3)) / 4.0f;

    for (int i = 0; i < buttonCount; ++i) {
        buttons[i].shape.setSize(sf::Vector2f(buttonWidth, 60));
        buttons[i].shape.setFillColor(sf::Color(100, 100, 100));
        buttons[i].shape.setOutlineColor(sf::Color(50, 50, 50));
        buttons[i].shape.setOutlineThickness(3);

        buttons[i].shape.setPosition(
            spacing + (spacing + buttonWidth) * i,
            SCREEN_HEIGHT * 0.8f
        );

        buttons[i].text.setFont(font);
        buttons[i].text.setString(buttonLabels[i]);
        buttons[i].text.setCharacterSize(30);
        buttons[i].text.setFillColor(sf::Color::White);

        sf::FloatRect textBounds = buttons[i].text.getLocalBounds();
        buttons[i].text.setOrigin(textBounds.left + textBounds.width / 2.0f,
            buttons[i].text.getOrigin().y);
        buttons[i].text.setPosition(buttons[i].shape.getPosition().x + buttonWidth / 2.0f,
            buttons[i].shape.getPosition().y + 12.5);

        buttons[i].isHovered = false;
    }
}

void createCancelConfirmationButtons(Button * buttons, int& buttonCount, sf::Font & font) {
    const char* buttonLabels[] = { "NO", "YES" };
    buttonCount = 2;

    float buttonWidth = 150;
    float totalButtonWidth = buttonWidth * 2.0f;
    float spacing = 200.0f;
    float startX = (SCREEN_WIDTH - (totalButtonWidth + spacing)) / 2.0f;

    for (int i = 0; i < buttonCount; ++i) {
        buttons[i].shape.setSize(sf::Vector2f(buttonWidth, 60));
        buttons[i].shape.setFillColor(sf::Color(100, 100, 100));
        buttons[i].shape.setOutlineColor(sf::Color(50, 50, 50));
        buttons[i].shape.setOutlineThickness(3);

        buttons[i].shape.setPosition(startX + (buttonWidth + spacing) * i,
            SCREEN_HEIGHT * 0.6f - 20.0f);

        buttons[i].text.setFont(font);
        buttons[i].text.setString(buttonLabels[i]);
        buttons[i].text.setCharacterSize(30);
        buttons[i].text.setFillColor(sf::Color::White);

        sf::FloatRect textBounds = buttons[i].text.getLocalBounds();
        buttons[i].text.setOrigin(textBounds.left + textBounds.width / 2.0f,
            buttons[i].text.getOrigin().y);
        buttons[i].text.setPosition(buttons[i].shape.getPosition().x + buttonWidth / 2.0f,
            buttons[i].shape.getPosition().y + 12.5);

        buttons[i].isHovered = false;
    }
}

void updateButtonColors(Button * buttons, int buttonCount, sf::RenderWindow & window) {
    sf::Vector2i mousePos = sf::Mouse::getPosition(window);

    for (int i = 0; i < buttonCount; ++i) {
        sf::FloatRect buttonBounds = buttons[i].shape.getGlobalBounds();
        if (buttonBounds.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y))) {
            buttons[i].shape.setFillColor(sf::Color(44, 48, 85));
            buttons[i].shape.setOutlineColor(sf::Color(29, 33, 70));
            buttons[i].isHovered = true;
        }
        else {
            buttons[i].shape.setFillColor(sf::Color(100, 100, 100));
            buttons[i].shape.setOutlineColor(sf::Color(50, 50, 50));
            buttons[i].isHovered = false;
        }
    }
}

// -------------------------------------------
//          Drawing Each Page
// -------------------------------------------
void drawMainMenu(sf::RenderWindow & window, sf::Font & font, Button * buttons, int buttonCount) {
    sf::Text mainTitle;
    mainTitle.setFont(font);
    mainTitle.setString("Electron");
    mainTitle.setCharacterSize(120);
    mainTitle.setFillColor(sf::Color::White);

    float titleYPosition = SCREEN_HEIGHT * 0.25f;
    sf::FloatRect titleBounds = mainTitle.getLocalBounds();
    mainTitle.setOrigin(titleBounds.left + titleBounds.width / 2.0f, titleBounds.top);
    mainTitle.setPosition(sf::Vector2f(SCREEN_WIDTH / 2.0f, titleYPosition));

    window.draw(mainTitle);

    for (int i = 0; i < buttonCount; ++i) {
        window.draw(buttons[i].shape);
        window.draw(buttons[i].text);
    }
}

void drawSettingsMenu(sf::RenderWindow & window, sf::Font & font, Button * buttons, int buttonCount) {
    sf::Text settingsTitle;
    settingsTitle.setFont(font);
    settingsTitle.setString("Settings");
    settingsTitle.setCharacterSize(100);
    settingsTitle.setFillColor(sf::Color::White);

    float titleYPosition = SCREEN_HEIGHT * 0.25f;
    sf::FloatRect titleBounds = settingsTitle.getLocalBounds();
    settingsTitle.setOrigin(titleBounds.left + titleBounds.width / 2.0f, titleBounds.top);
    settingsTitle.setPosition(sf::Vector2f(SCREEN_WIDTH / 2.0f, titleYPosition));

    window.draw(settingsTitle);

    for (int i = 0; i < buttonCount; ++i) {
        window.draw(buttons[i].shape);
        window.draw(buttons[i].text);
    }
}

void drawInformationPage(sf::RenderWindow & window, sf::Font & font, const sf::Text & infoText,
    Button * buttons, int buttonCount) {
    sf::Text title;
    title.setFont(font);
    title.setString("Information");
    title.setCharacterSize(60);
    title.setFillColor(sf::Color::White);
    title.setPosition(SCREEN_WIDTH / 2.0f - 150.0f, 50.0f);

    window.draw(title);
    window.draw(infoText);

    for (int i = 0; i < buttonCount; ++i) {
        window.draw(buttons[i].shape);
        window.draw(buttons[i].text);
    }
}

/**
 * We use a special "clipping" view for the text box.
 * We also draw selection highlight if there's a valid selection.
 */
void drawNewComponentPage(sf::RenderWindow & window, sf::Font & font,
    Button * buttons, int buttonCount,
    TextInputBox & inputBox) {
    sf::Text title;
    title.setFont(font);
    title.setString("Create New Component");
    title.setCharacterSize(60);
    title.setFillColor(sf::Color::White);

    sf::FloatRect titleBounds = title.getLocalBounds();
    title.setOrigin(titleBounds.left + titleBounds.width / 2.0f, titleBounds.top);
    title.setPosition(SCREEN_WIDTH / 2.0f, 50.0f);

    // Left rectangle (placeholder)
    sf::RectangleShape leftRect(sf::Vector2f(SCREEN_WIDTH * 0.4f, 400.0f));
    leftRect.setPosition(SCREEN_WIDTH * 0.05f, SCREEN_HEIGHT * 0.3f);
    leftRect.setFillColor(sf::Color(50, 50, 50));

    // Configure the text box shape on the right
    inputBox.shape.setSize(sf::Vector2f(SCREEN_WIDTH * 0.4f, 400.0f));
    inputBox.shape.setPosition(SCREEN_WIDTH * 0.55f, SCREEN_HEIGHT * 0.3f);
    inputBox.shape.setFillColor(sf::Color(230, 230, 230));
    inputBox.shape.setOutlineThickness(2.0f);
    inputBox.shape.setOutlineColor(sf::Color::Black);

    // Draw non-clipped parts
    window.draw(title);
    window.draw(leftRect);
    window.draw(inputBox.shape);

    // Save old view
    sf::View oldView = window.getView();

    // Build text-box (clipped) view
    sf::FloatRect textBoxRect = inputBox.shape.getGlobalBounds();
    sf::View textBoxView;
    textBoxView.setSize(textBoxRect.width, textBoxRect.height);
    textBoxView.setCenter(
        textBoxRect.left + textBoxRect.width / 2.0f,
        textBoxRect.top + textBoxRect.height / 2.0f
    );
    float windowWidth = static_cast<float>(window.getSize().x);
    float windowHeight = static_cast<float>(window.getSize().y);
    sf::FloatRect viewport(
        textBoxRect.left / windowWidth,
        textBoxRect.top / windowHeight,
        textBoxRect.width / windowWidth,
        textBoxRect.height / windowHeight
    );
    textBoxView.setViewport(viewport);
    window.setView(textBoxView);

    // Draw selection highlight if we have a valid selection
    if (inputBox.selectionStart != inputBox.selectionEnd) {
        window.draw(inputBox.selectionHighlight);
    }

    // Then draw the text and cursor
    window.draw(inputBox.displayText);
    if (inputBox.isActive) {
        window.draw(inputBox.cursorShape);
    }

    // Restore old view
    window.setView(oldView);

    // Draw scrollbar & bottom buttons
    window.draw(inputBox.scrollBarTrack);
    window.draw(inputBox.scrollBarThumb);

    for (int i = 0; i < buttonCount; ++i) {
        window.draw(buttons[i].shape);
        window.draw(buttons[i].text);
    }
}

// -------------------------------------------
//    Drawing Cancel Confirmation Popup
// -------------------------------------------
void drawCancelConfirmationPopup(sf::RenderWindow & window, sf::Font & font,
    Button * buttons, int buttonCount) {
    sf::RectangleShape popup(sf::Vector2f(SCREEN_WIDTH * 0.4f, SCREEN_HEIGHT * 0.3f));
    popup.setPosition(SCREEN_WIDTH * 0.3f, SCREEN_HEIGHT * 0.35f);
    popup.setFillColor(sf::Color(50, 50, 50));

    sf::Text confirmText;
    confirmText.setFont(font);
    confirmText.setString("Are you sure you want to cancel the process?");
    confirmText.setCharacterSize(30);
    confirmText.setFillColor(sf::Color::White);

    sf::FloatRect textBounds = confirmText.getLocalBounds();
    confirmText.setOrigin(textBounds.left + textBounds.width / 2.0f, confirmText.getOrigin().y);
    confirmText.setPosition(popup.getPosition().x + popup.getSize().x / 2.0f,
        popup.getPosition().y + popup.getSize().y * 0.3f);

    window.draw(popup);
    window.draw(confirmText);

    for (int i = 0; i < buttonCount; ++i) {
        window.draw(buttons[i].shape);
        window.draw(buttons[i].text);
    }
}

// -------------------------------------------
//     Page Interaction Handlers
// -------------------------------------------
void handleMainMenuInteractions(const sf::Event & event, sf::RenderWindow & window, Button * buttons, int buttonCount,
    bool& running, ApplicationPage & currentPage) {
    if (pageTransitionClock.getElapsedTime().asSeconds() < PAGE_TRANSITION_COOLDOWN) {
        return;
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);

        for (int i = 0; i < buttonCount; ++i) {
            sf::FloatRect buttonBounds = buttons[i].shape.getGlobalBounds();
            if (buttonBounds.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y))) {
                std::string buttonText = buttons[i].text.getString();
                if (buttonText == "Exit") {
                    running = false;
                    run = false;
                    break;
                }
                else if (buttonText == "Settings") {
                    currentPage = ApplicationPage::Settings;
                    pageTransitionClock.restart();
                    break;
                }
                else if (buttonText == "Information") {
                    currentPage = ApplicationPage::Information;
                    pageTransitionClock.restart();
                    break;
                }
                else if (buttonText == "New Project") {
                    isInMenu = false;
                }
                else if ("Open Project") {
                    loadProject = true;
                    isInMenu = false;
                }
                // Additional buttons like "New Project" and "Open Project" can be handled here
            }
        }
    }
}

void handleSettingsInteractions(const sf::Event & event, sf::RenderWindow & window, Button * buttons, int buttonCount,
    ApplicationPage & currentPage) {
    if (pageTransitionClock.getElapsedTime().asSeconds() < PAGE_TRANSITION_COOLDOWN) {
        return;
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);

        for (int i = 0; i < buttonCount; ++i) {
            sf::FloatRect buttonBounds = buttons[i].shape.getGlobalBounds();
            if (buttonBounds.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y))) {
                std::string buttonText = buttons[i].text.getString();
                if (buttonText == "Back") {
                    currentPage = ApplicationPage::MainMenu;
                    pageTransitionClock.restart();
                    break;
                }
                else if (buttonText == "New Component") {
                    currentPage = ApplicationPage::NewComponent;
                    pageTransitionClock.restart();
                    refreshPiece = new PieceDrawer("refreshPiece.txt");
                    break;
                }
                // Handle "HotKeys" button here if needed
            }
        }
    }
}

void handleInformationInteractions(const sf::Event & event, sf::RenderWindow & window, Button * buttons, int buttonCount,
    ApplicationPage & currentPage) {
    if (pageTransitionClock.getElapsedTime().asSeconds() < PAGE_TRANSITION_COOLDOWN) {
        return;
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);

        for (int i = 0; i < buttonCount; ++i) {
            sf::FloatRect buttonBounds = buttons[i].shape.getGlobalBounds();
            if (buttonBounds.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y))) {
                std::string buttonText = buttons[i].text.getString();
                if (buttonText == "Back") {
                    currentPage = ApplicationPage::MainMenu;
                    pageTransitionClock.restart();
                    break;
                }
            }
        }
    }
}

// *** NEW COMPONENT PAGE INTERACTIONS ***
void handleNewComponentInteractions(const sf::Event & event, sf::RenderWindow & window, Button * buttons, int buttonCount,
    ApplicationPage & currentPage, TextInputBox & inputBox) {
    if (pageTransitionClock.getElapsedTime().asSeconds() < PAGE_TRANSITION_COOLDOWN) {
        return;
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2i mousePosI = sf::Mouse::getPosition(window);
        sf::Vector2f mousePosF(static_cast<float>(mousePosI.x), static_cast<float>(mousePosI.y));

        bool clickedInsideTextBox = inputBox.shape.getGlobalBounds().contains(mousePosF) ||
            inputBox.scrollBarThumb.getGlobalBounds().contains(mousePosF) ||
            inputBox.scrollBarTrack.getGlobalBounds().contains(mousePosF);

        if (clickedInsideTextBox) {
            inputBox.isActive = true;
            inputBox.isSelecting = true;

            // Determine character index based on mouse position relative to the text box
            if (inputBox.displayText.getFont()) {
                sf::Vector2f relativePos = mousePosF - inputBox.displayText.getPosition();
                int charIndex = getCharIndexAtPos(inputBox, relativePos, *(inputBox.displayText.getFont()));
                inputBox.selectionStart = charIndex;
                inputBox.selectionEnd = charIndex;
            }
        }
        else {
            inputBox.isActive = false;
        }

        // Scroll bar dragging
        if (inputBox.scrollBarThumb.getGlobalBounds().contains(mousePosF)) {
            inputBox.isDraggingThumb = true;
            inputBox.thumbClickOffset = mousePosF.y - inputBox.scrollBarThumb.getPosition().y;
        }
        else if (inputBox.scrollBarTrack.getGlobalBounds().contains(mousePosF)) {
            float newThumbY = mousePosF.y - inputBox.scrollBarThumb.getSize().y / 2.0f;
            float trackTop = inputBox.scrollBarTrack.getPosition().y;
            float trackBottom = trackTop + inputBox.scrollBarTrack.getSize().y - inputBox.scrollBarThumb.getSize().y;

            if (newThumbY < trackTop) newThumbY = trackTop;
            if (newThumbY > trackBottom) newThumbY = trackBottom;

            inputBox.scrollBarThumb.setPosition(inputBox.scrollBarThumb.getPosition().x, newThumbY);

            float thumbRatio = (newThumbY - trackTop) / (trackBottom - trackTop);
            inputBox.scrollOffset = thumbRatio * (inputBox.contentHeight - inputBox.shape.getSize().y + 20.0f);
        }

        // Check buttons
        for (int i = 0; i < buttonCount; ++i) {
            sf::FloatRect buttonBounds = buttons[i].shape.getGlobalBounds();
            if (buttonBounds.contains(static_cast<float>(mousePosI.x), static_cast<float>(mousePosI.y))) {
                std::string buttonText = buttons[i].text.getString();
                if (buttonText == "Cancel") {
                    currentPage = ApplicationPage::CancelConfirmation;
                    pageTransitionClock.restart();
                    break;
                }
                else if (buttonText == "Create") {
                    currentPage = ApplicationPage::Settings;
                    pageTransitionClock.restart();
                    std::string pieceName = refreshPiece->piece.name;
                    pieceName += ".PS";
                    std::ofstream outFile(pieceName);
                    if (!outFile) {
                        std::cerr << "Error when creating file: " << pieceName << std::endl;
                    }
                    outFile << componentNameBox.inputText;
                    outFile.close();
                    std::ofstream out("piece.txt", std::ios::app);
                    out << '\n' << refreshPiece->piece.name + ".PS";
                    out.close();
                    break;
                }
                else if (buttonText == "Refresh") {
                    delete refreshPiece;
                    std::ofstream outFile("refreshPiece.txt", std::ios::out | std::ios::trunc);
                    if (!outFile.is_open()) {
                        std::cerr << "Error opening file: " << "refreshPiece.txt" << std::endl;
                        exit(-1);
                    }
                    outFile << componentNameBox.inputText;
                    outFile.close();
                    refreshPiece = new PieceDrawer("refreshPiece.txt");

                    break;
                }
            }
        }
    }
    else if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left) {
        inputBox.isDraggingThumb = false;
        inputBox.isSelecting = false;
    }
}

// -------------------------------------------
//     Cancel Confirmation Interactions
// -------------------------------------------
void handleCancelConfirmationInteractions(const sf::Event & event, sf::RenderWindow & window, Button * buttons,
    int buttonCount, ApplicationPage & currentPage) {
    if (pageTransitionClock.getElapsedTime().asSeconds() < PAGE_TRANSITION_COOLDOWN) {
        return;
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);

        for (int i = 0; i < buttonCount; ++i) {
            sf::FloatRect buttonBounds = buttons[i].shape.getGlobalBounds();
            if (buttonBounds.contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y))) {
                std::string buttonText = buttons[i].text.getString();
                if (buttonText == "NO") {
                    currentPage = ApplicationPage::NewComponent;
                    pageTransitionClock.restart();
                    break;
                }
                else if (buttonText == "YES") {
                    currentPage = ApplicationPage::Settings;
                    pageTransitionClock.restart();
                    delete refreshPiece;
                    refreshPiece = nullptr; // Prevent dangling pointer
                    break;
                }
            }
        }
    }
}

// -------------------------------------------
//         Handle Events Function
// -------------------------------------------
void handleGlobalEvents(const sf::Event & event, bool& running) {
    // Handle window closed and escape key globally
    if (event.type == sf::Event::Closed) {
        run = false;
    }
    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
        run = false;
    }
}

// -------------------------------------------
//   Loading and Wrapping Information Text
// -------------------------------------------
sf::Text loadInformationContent(sf::Font & font, const std::string & filePath) {
    const float LEFT_MARGIN = 100.0f;
    const float RIGHT_MARGIN = 100.0f;
    const float MAX_WIDTH = SCREEN_WIDTH - (LEFT_MARGIN + RIGHT_MARGIN);

    sf::Text infoText;
    infoText.setFont(font);
    infoText.setCharacterSize(30);
    infoText.setFillColor(sf::Color::White);

    std::ifstream file(filePath);
    if (file) {
        std::string line;
        std::string wrappedContent;

        while (std::getline(file, line)) {
            if (line.empty()) {
                wrappedContent += "\n";
                continue;
            }

            sf::Text testText;
            testText.setFont(font);
            testText.setCharacterSize(30);
            testText.setString(line);

            if (testText.getLocalBounds().width > MAX_WIDTH) {
                std::istringstream lineStream(line);
                std::string word;
                std::string currentLine;

                while (lineStream >> word) {
                    sf::Text wordTestText;
                    wordTestText.setFont(font);
                    wordTestText.setCharacterSize(30);
                    wordTestText.setString(currentLine + (currentLine.empty() ? "" : " ") + word);

                    if (wordTestText.getLocalBounds().width > MAX_WIDTH && !currentLine.empty()) {
                        wrappedContent += currentLine + "\n";
                        currentLine = word;
                    }
                    else {
                        if (!currentLine.empty()) currentLine += " ";
                        currentLine += word;
                    }
                }
                if (!currentLine.empty()) {
                    wrappedContent += currentLine + "\n";
                }
            }
            else {
                wrappedContent += line + "\n";
            }
        }
        if (!wrappedContent.empty() && wrappedContent.back() == '\n') {
            wrappedContent.pop_back();
        }
        infoText.setString(wrappedContent);
    }
    else {
        infoText.setString("Error: Could not load information.");
    }
    infoText.setPosition(LEFT_MARGIN, 200.0f);
    return infoText;
}

// -------------------------------------------
//     Handling Raw Text Entry + Selection
// -------------------------------------------
void handleTextInput(TextInputBox & inputBox, const sf::Event & event) {
    if (!inputBox.isActive) return;

    if (event.type == sf::Event::TextEntered) {
        if (event.text.unicode == 8) { // Backspace
            if (inputBox.selectionStart != inputBox.selectionEnd) {
                int start = std::min(inputBox.selectionStart, inputBox.selectionEnd);
                int end = std::max(inputBox.selectionStart, inputBox.selectionEnd);
                inputBox.inputText.erase(start, end - start);
                inputBox.selectionStart = start;
                inputBox.selectionEnd = start;
            }
            else if (inputBox.selectionStart > 0) {
                inputBox.inputText.erase(inputBox.selectionStart - 1, 1);
                inputBox.selectionStart--;
                inputBox.selectionEnd = inputBox.selectionStart;
            }
        }
        else if (event.text.unicode < 128) { // Handle standard ASCII characters
            if (inputBox.selectionStart != inputBox.selectionEnd) {
                int start = std::min(inputBox.selectionStart, inputBox.selectionEnd);
                int end = std::max(inputBox.selectionStart, inputBox.selectionEnd);
                inputBox.inputText.erase(start, end - start);
                inputBox.selectionStart = start;
                inputBox.selectionEnd = start;
            }
            if (event.text.unicode == 13) { // Enter key
                inputBox.inputText.insert(inputBox.selectionStart, "\n");
                inputBox.selectionStart++;
                inputBox.selectionEnd = inputBox.selectionStart;
            }
            else {
                char character = static_cast<char>(event.text.unicode);
                inputBox.inputText.insert(inputBox.selectionStart, 1, character);
                inputBox.selectionStart++;
                inputBox.selectionEnd = inputBox.selectionStart;
            }
            inputBox.displayText.setString(inputBox.inputText);
        }
    }
    // **Removed Clipboard Paste Handling**
    // The following code has been removed to eliminate the Ctrl+V feature:
    /*
    else if (event.type == sf::Event::KeyPressed) {
        // Handle clipboard paste
        bool ctrlDown = (sf::Keyboard::isKeyPressed(sf::Keyboard::LControl) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::RControl));
#ifdef __APPLE__
        bool cmdDown = (sf::Keyboard::isKeyPressed(sf::Keyboard::LSystem) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::RSystem));
        if (cmdDown && event.key.code == sf::Keyboard::V) {
            sf::String clipboardContent = sf::Clipboard::getString();
            if (clipboardContent.getSize() > 0) {
                if (inputBox.selectionStart != inputBox.selectionEnd) {
                    int start = std::min(inputBox.selectionStart, inputBox.selectionEnd);
                    int end = std::max(inputBox.selectionStart, inputBox.selectionEnd);
                    inputBox.inputText.erase(start, end - start);
                    inputBox.selectionStart = start;
                    inputBox.selectionEnd = start;
                }
                inputBox.inputText.insert(inputBox.selectionStart, clipboardContent.toAnsiString());
                inputBox.selectionStart += clipboardContent.getSize();
                inputBox.selectionEnd = inputBox.selectionStart;
                inputBox.displayText.setString(inputBox.inputText);
            }
        }
#else
        if (ctrlDown && event.key.code == sf::Keyboard::V) {
            sf::String clipboardContent = sf::Clipboard::getString();
            if (clipboardContent.getSize() > 0) {
                if (inputBox.selectionStart != inputBox.selectionEnd) {
                    int start = std::min(inputBox.selectionStart, inputBox.selectionEnd);
                    int end = std::max(inputBox.selectionStart, inputBox.selectionEnd);
                    inputBox.inputText.erase(start, end - start);
                    inputBox.selectionStart = start;
                    inputBox.selectionEnd = start;
                }
                inputBox.inputText.insert(inputBox.selectionStart, clipboardContent.toAnsiString());
                inputBox.selectionStart += clipboardContent.getSize();
                inputBox.selectionEnd = inputBox.selectionStart;
                inputBox.displayText.setString(inputBox.inputText);
            }
        }
#endif
    }
    */
}

// -------------------------------------------
//  Update Text Input Box (wrapping, scroll, etc.)
// -------------------------------------------
void updateTextInputBox(TextInputBox & inputBox, sf::Font & font, float deltaTime) {
    inputBox.displayText.setFont(font);
    inputBox.displayText.setCharacterSize(30);
    inputBox.displayText.setFillColor(sf::Color::Black);

    float maxWidth = inputBox.shape.getSize().x - 20.0f;
    std::string wrappedText;

    // Wrap line by line
    std::istringstream stream(inputBox.inputText);
    std::string line;
    while (std::getline(stream, line)) {
        std::istringstream lineStream(line);
        std::string word;
        std::string currentLine;

        while (lineStream >> word) {
            sf::Text tempText;
            tempText.setFont(font);
            tempText.setCharacterSize(30);

            std::string testLine = currentLine;
            if (!testLine.empty()) testLine += " ";
            testLine += word;
            tempText.setString(testLine);

            if (tempText.getLocalBounds().width > maxWidth && !currentLine.empty()) {
                wrappedText += currentLine + "\n";
                currentLine = word;
            }
            else {
                if (!currentLine.empty()) currentLine += " ";
                currentLine += word;
            }
        }
        wrappedText += currentLine + "\n";
    }
    if (!wrappedText.empty() && wrappedText.back() == '\n') {
        wrappedText.pop_back();
    }

    inputBox.displayText.setString(wrappedText);

    // Measure content height
    int numLines = 1;
    for (char c : wrappedText) {
        if (c == '\n') ++numLines;
    }
    float lineHeight = 35.0f; // ~30 font + spacing
    inputBox.contentHeight = numLines * lineHeight;

    float visibleHeight = inputBox.shape.getSize().y - 20.0f;
    if (inputBox.contentHeight < visibleHeight) {
        inputBox.contentHeight = visibleHeight;
    }

    // Clamp scroll offset
    float maxScroll = inputBox.contentHeight - visibleHeight;
    if (inputBox.scrollOffset < 0.0f) {
        inputBox.scrollOffset = 0.0f;
    }
    if (inputBox.scrollOffset > maxScroll) {
        inputBox.scrollOffset = maxScroll;
    }

    // Position the text
    inputBox.displayText.setPosition(
        inputBox.shape.getPosition().x + 10.0f,
        inputBox.shape.getPosition().y + 10.0f - inputBox.scrollOffset
    );

    // Position the cursor near the end of the unwrapped text
    {
        size_t lastNewline = inputBox.inputText.find_last_of('\n');
        std::string lastLine;
        if (lastNewline == std::string::npos) {
            lastLine = inputBox.inputText;
        }
        else {
            lastLine = inputBox.inputText.substr(lastNewline + 1);
        }

        sf::Text tempText;
        tempText.setFont(font);
        tempText.setCharacterSize(30);
        tempText.setString(lastLine);

        float xPos = inputBox.shape.getPosition().x + 10.0f + tempText.getLocalBounds().width;
        float yPos = inputBox.shape.getPosition().y + 10.0f + (numLines - 1) * lineHeight - inputBox.scrollOffset;

        inputBox.cursorShape.setSize(sf::Vector2f(2.0f, 30.0f));
        inputBox.cursorShape.setPosition(xPos, yPos);
        inputBox.cursorShape.setFillColor(sf::Color::Black);
    }

    // Cursor blinking
    inputBox.cursorBlinkTimer += deltaTime;
    if (inputBox.cursorBlinkTimer > 0.5f) {
        inputBox.cursorBlinkTimer = 0.0f;
        if (inputBox.cursorShape.getFillColor() == sf::Color::Black) {
            inputBox.cursorShape.setFillColor(sf::Color::Transparent);
        }
        else {
            inputBox.cursorShape.setFillColor(sf::Color::Black);
        }
    }

    // Update scroll bar
    float barWidth = 10.0f;
    inputBox.scrollBarTrack.setPosition(
        inputBox.shape.getPosition().x + inputBox.shape.getSize().x - barWidth,
        inputBox.shape.getPosition().y
    );
    inputBox.scrollBarTrack.setSize(sf::Vector2f(barWidth, inputBox.shape.getSize().y));
    inputBox.scrollBarTrack.setFillColor(sf::Color(200, 200, 200));

    float ratio = visibleHeight / inputBox.contentHeight;
    float thumbHeight = inputBox.scrollBarTrack.getSize().y * ratio;
    if (thumbHeight < 20.0f) {
        thumbHeight = 20.0f;
    }

    float trackTop = inputBox.scrollBarTrack.getPosition().y;
    float trackBottom = trackTop + inputBox.scrollBarTrack.getSize().y - thumbHeight;
    float thumbY = trackTop;
    if (maxScroll > 0.0f) {
        thumbY += (inputBox.scrollOffset / maxScroll) * (trackBottom - trackTop);
    }

    inputBox.scrollBarThumb.setPosition(inputBox.scrollBarTrack.getPosition().x, thumbY);
    inputBox.scrollBarThumb.setSize(sf::Vector2f(barWidth, thumbHeight));
    inputBox.scrollBarThumb.setFillColor(sf::Color(100, 100, 100));

    // Update selection highlight (simple single-line logic)
    if (inputBox.displayText.getFont()) {
        // Calculate selection positions relative to the displayText
        int start = std::min(inputBox.selectionStart, inputBox.selectionEnd);
        int end = std::max(inputBox.selectionStart, inputBox.selectionEnd);
        if (start == end) {
            // No selection
            inputBox.selectionHighlight.setSize(sf::Vector2f(0, 0));
        }
        else {
            // Approximate selection for single-line
            sf::Text temp;
            temp.setFont(font);
            temp.setCharacterSize(30);
            temp.setString(inputBox.inputText.substr(0, start));
            float leftWidth = temp.getLocalBounds().width;

            sf::Text tempSelected;
            tempSelected.setFont(font);
            tempSelected.setCharacterSize(30);
            tempSelected.setString(inputBox.inputText.substr(start, end - start));
            float selectedWidth = tempSelected.getLocalBounds().width;

            float xPos = inputBox.displayText.getPosition().x + leftWidth;
            float yPos = inputBox.displayText.getPosition().y;
            float height = 35.0f; // line height

            inputBox.selectionHighlight.setPosition(xPos, yPos);
            inputBox.selectionHighlight.setSize(sf::Vector2f(selectedWidth, height));
        }
    }
}

// -------------------------------------------
//  Handle Text Box Scrolling (Mouse Wheel + Thumb Drag)
// -------------------------------------------
void handleTextBoxScrolling(TextInputBox & inputBox, const sf::Event & event) {
    if (event.type == sf::Event::MouseWheelScrolled) {
        // Scroll up/down if within box
        sf::Vector2i mousePosI = sf::Mouse::getPosition();
        sf::Vector2f mousePosF(static_cast<float>(mousePosI.x), static_cast<float>(mousePosI.y));
        if (inputBox.shape.getGlobalBounds().contains(mousePosF) ||
            inputBox.scrollBarThumb.getGlobalBounds().contains(mousePosF) ||
            inputBox.scrollBarTrack.getGlobalBounds().contains(mousePosF)) {
            float delta = event.mouseWheelScroll.delta;
            inputBox.scrollOffset -= delta * 30.0f;
        }
    }
    else if (event.type == sf::Event::MouseMoved) {
        if (inputBox.isDraggingThumb) {
            float newThumbY = static_cast<float>(event.mouseMove.y) - inputBox.thumbClickOffset;

            float trackTop = inputBox.scrollBarTrack.getPosition().y;
            float trackBottom = trackTop + inputBox.scrollBarTrack.getSize().y - inputBox.scrollBarThumb.getSize().y;

            if (newThumbY < trackTop) newThumbY = trackTop;
            if (newThumbY > trackBottom) newThumbY = trackBottom;

            inputBox.scrollBarThumb.setPosition(inputBox.scrollBarThumb.getPosition().x, newThumbY);

            float thumbRatio = (newThumbY - trackTop) / (trackBottom - trackTop);
            inputBox.scrollOffset = thumbRatio * (inputBox.contentHeight - inputBox.shape.getSize().y + 20.0f);
        }
    }
}

// -------------------------------------------
//    Get approximate character index based
//    on mouse position in the text box
// -------------------------------------------
int getCharIndexAtPos(const TextInputBox & inputBox, const sf::Vector2f & relativePos, const sf::Font & font) {
    // Simple approach: iterate through characters and find the closest position
    float currentWidth = 0.f;
    int index = 0;
    for (; index < static_cast<int>(inputBox.inputText.size()); index++) {
        sf::Text temp;
        temp.setFont(font);
        temp.setCharacterSize(30);
        temp.setString(inputBox.inputText[index]);

        float charWidth = temp.getLocalBounds().width;

        if (currentWidth + charWidth / 2.0f >= relativePos.x) {
            break;
        }
        currentWidth += charWidth;
    }
    return index;
}
