#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

static float vlen(sf::Vector2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }
static sf::Vector2f vnorm(sf::Vector2f v) {
    float l = vlen(v);
    if (l <= 1e-6f) return {0.f, 0.f};
    return {v.x / l, v.y / l};
}
static float vdot(sf::Vector2f a, sf::Vector2f b) { return a.x * b.x + a.y * b.y; }
static sf::Vector2f vperp(sf::Vector2f v) { return {-v.y, v.x}; }

struct Particle {
    std::string name;
    sf::Vector2f pos;
    sf::Vector2f vel;     // momentum direction is normalized vel
    sf::Vector2f spinDir; // spin direction unit vector
    float radius = 8.f;
    sf::Color color = sf::Color::White;

    std::vector<sf::Vector2f> trail;
    float trailTimer = 0.f;
};

struct DecayEvent {
    Particle electron;
    Particle antinu;
    int protonSpinSign = 0; // toy +1 or -1
    int neutronSpinSign = +1;
    int L_needed = 0;       // toy orbital term
    float timeAlive = 0.f;
    float duration = 3.0f;
};

enum class Mode {
    SpinOnly = 1,      // deliberately oversimplified: "spins always cancel"
    SpinAndMotion = 2, // show momentum + helicity
    FullConservation = 3 // show orbital placeholder L_needed
};

struct Tooltip {
    sf::Vector2f pos{};
    std::string title;
    std::string body;
    bool active = false;
};

static float dist2(sf::Vector2f a, sf::Vector2f b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static bool hitCircle(sf::Vector2f mouse, sf::Vector2f center, float r) {
    return dist2(mouse, center) <= (r * r);
}

static void drawLabel(sf::RenderTarget& rt, const sf::Font& font, sf::Vector2f at, const std::string& s) {
    sf::Text t(font);
    t.setCharacterSize(14);
    t.setFillColor(sf::Color(245, 245, 245, 220));
    t.setOutlineThickness(2.f);
    t.setOutlineColor(sf::Color(0, 0, 0, 180));
    t.setString(s);

    auto b = t.getLocalBounds();
    t.setOrigin(sf::Vector2f{b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f});
    t.setPosition(at);
    rt.draw(t);
}

static void drawTooltipBox(sf::RenderTarget& rt, const sf::Font& font, sf::Vector2f mousePos, const std::string& title, const std::string& body) {
    sf::Text tt(font);
    tt.setCharacterSize(16);
    tt.setFillColor(sf::Color(240, 240, 240));
    tt.setString(title);

    sf::Text tb(font);
    tb.setCharacterSize(15);
    tb.setFillColor(sf::Color(220, 220, 220));
    tb.setString(body);

    auto bt = tt.getLocalBounds();
    auto bb = tb.getLocalBounds();

    float pad = 10.f;
    float w = std::max(bt.size.x, bb.size.x) + pad * 2.f;
    float h = (bt.size.y + bb.size.y) + pad * 3.f;

    sf::Vector2f boxPos = mousePos + sf::Vector2f{16.f, 16.f};

    // Keep inside window-ish bounds (simple clamp)
    if (boxPos.x + w > 1080.f) boxPos.x = 1080.f - w;
    if (boxPos.y + h > 680.f) boxPos.y = 680.f - h;
    if (boxPos.x < 10.f) boxPos.x = 10.f;
    if (boxPos.y < 10.f) boxPos.y = 10.f;

    sf::RectangleShape r(sf::Vector2f{w, h});
    r.setPosition(boxPos);
    r.setFillColor(sf::Color(10, 12, 16, 230));
    r.setOutlineThickness(1.f);
    r.setOutlineColor(sf::Color(90, 100, 125, 200));
    rt.draw(r);

    tt.setPosition(boxPos + sf::Vector2f{pad, pad});
    tb.setPosition(boxPos + sf::Vector2f{pad, pad * 2.f + bt.size.y});

    rt.draw(tt);
    rt.draw(tb);
}

static float pointSegmentDistance(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b) {
    sf::Vector2f ab = b - a;
    float ab2 = vdot(ab, ab);
    if (ab2 <= 1e-6f) return vlen(p - a);
    float t = vdot(p - a, ab) / ab2;
    t = std::clamp(t, 0.f, 1.f);
    sf::Vector2f proj = a + ab * t;
    return vlen(p - proj);
}


static int signf(float x) { return (x >= 0.f) ? 1 : -1; }

static int helicitySign(const sf::Vector2f& spinDir, const sf::Vector2f& momDir) {
    // helicity sign is sign(spin dot momentum)
    return signf(vdot(spinDir, momDir));
}

static void drawArrow(sf::RenderTarget& rt, sf::Vector2f from, sf::Vector2f dirUnit, float L, sf::Color col, float head = 10.f) {
    sf::Vector2f to = from + dirUnit * L;

    sf::Vertex line[2] = {
        sf::Vertex{from, col},
        sf::Vertex{to, col},
    };
    rt.draw(line, 2, sf::PrimitiveType::Lines);

    sf::Vector2f p = vperp(dirUnit);
    sf::Vector2f h1 = to - dirUnit * head + p * (head * 0.55f);
    sf::Vector2f h2 = to - dirUnit * head - p * (head * 0.55f);

    sf::Vertex headLines[4] = {
        sf::Vertex{to, col}, sf::Vertex{h1, col},
        sf::Vertex{to, col}, sf::Vertex{h2, col},
    };
    rt.draw(headLines, 4, sf::PrimitiveType::Lines);
}

static void drawGlowCircle(sf::RenderTarget& rt, sf::Vector2f center, float r, sf::Color c) {
    for (int i = 5; i >= 1; --i) {
        float rr = r + i * 6.f;
        sf::CircleShape s(rr);
        s.setOrigin(sf::Vector2f{rr, rr});
        sf::Color cc = c;
        cc.a = static_cast<std::uint8_t>(18 * i);
        s.setFillColor(cc);
        s.setPosition(center);
        rt.draw(s);
    }

    sf::CircleShape core(r);
    core.setOrigin(sf::Vector2f{r, r});
    core.setFillColor(c);
    core.setPosition(center);
    rt.draw(core);
}

static void drawTrail(sf::RenderTarget& rt, const Particle& p) {
    if (p.trail.size() < 2) return;

    sf::VertexArray va(sf::PrimitiveType::LineStrip, p.trail.size());
    for (std::size_t i = 0; i < p.trail.size(); ++i) {
        float t = static_cast<float>(i) / static_cast<float>(p.trail.size() - 1);
        sf::Color c = p.color;
        c.a = static_cast<std::uint8_t>(40 + 140 * t);
        va[i].position = p.trail[i];
        va[i].color = c;
    }
    rt.draw(va);
}

static void drawOrbitalSwirl(sf::RenderTarget& rt, sf::Vector2f center, int L_needed, float time) {
    int mag = std::abs(L_needed);
    if (mag == 0) return;

    float baseR = 22.f;
    float r = baseR + mag * 10.f;

    sf::VertexArray va(sf::PrimitiveType::LineStrip);
    int points = 140;
    float turns = 2.0f + 0.5f * mag;
    float phase = time * 2.2f * (L_needed > 0 ? 1.f : -1.f);

    sf::Color col = (L_needed == 0)
        ? sf::Color(120, 220, 140, 130)
        : sf::Color(230, 120, 120, static_cast<std::uint8_t>(40 + mag * 20));

    for (int i = 0; i <= points; ++i) {
        float a = (static_cast<float>(i) / points) * (2.f * 3.1415926f) * turns + phase;
        float rr = r + std::sin(a * 1.2f) * 5.f;
        sf::Vector2f pos(center.x + std::cos(a) * rr, center.y + std::sin(a) * rr);
        va.append(sf::Vertex{pos, col});
    }
    rt.draw(va);
}

static DecayEvent makeEvent(std::mt19937& rng, sf::Vector2f origin, float leftHandBias, Mode mode) {
    std::uniform_real_distribution<float> u01(0.f, 1.f);
    std::uniform_real_distribution<float> angleDist(-0.35f, 0.35f);
    std::uniform_int_distribution<int> pm01(0, 1);

    DecayEvent ev;
    ev.neutronSpinSign = +1;

    // Mostly rightward electron momentum
    float a = angleDist(rng);
    sf::Vector2f dirE(std::cos(a), std::sin(a));
    dirE = vnorm(dirE);
    sf::Vector2f dirNu = vnorm(-dirE);

    // Electron spin: biased left-handed (spin opposite momentum) for Mode >= 2
    bool wantLeft = (u01(rng) < leftHandBias);
    sf::Vector2f spinE = wantLeft ? vnorm(-dirE) : vnorm(dirE);

    // Anti-neutrino forced right-handed (spin aligned with its momentum) for Mode >= 2
    sf::Vector2f spinNu = vnorm(dirNu);

    ev.electron.name = "e-";
    ev.electron.pos = origin;
    ev.electron.vel = dirE * 260.f;
    ev.electron.spinDir = spinE;
    ev.electron.radius = 8.f;
    ev.electron.color = sf::Color(240, 210, 80);

    ev.antinu.name = "anti-nu";
    ev.antinu.pos = origin;
    ev.antinu.vel = dirNu * 260.f;
    ev.antinu.spinDir = spinNu;
    ev.antinu.radius = 6.f;
    ev.antinu.color = sf::Color(120, 190, 255);

    ev.protonSpinSign = pm01(rng) ? +1 : -1;

    // MODE 1: enforce the oversimplified myth visually: spins are always opposite.
    // Hide the real relationship between helicity and motion by construction.
    if (mode == Mode::SpinOnly) {
        // Keep motion for animation, but force spin cancellation in "space":
        ev.antinu.spinDir = vnorm(-ev.electron.spinDir);
    }

    // Toy integer bookkeeping for L_needed (used in Mode 3 as "orbital placeholder")
    int sP = ev.protonSpinSign;
    int sE = (ev.electron.spinDir.y >= 0.f) ? +1 : -1;
    int sN = (ev.antinu.spinDir.y >= 0.f) ? +1 : -1;
    ev.L_needed = ev.neutronSpinSign - (sP + sE + sN);

    return ev;
}

static sf::RectangleShape hudPanel(sf::Vector2f pos, sf::Vector2f size) {
    sf::RectangleShape r(size);
    r.setPosition(pos);
    r.setFillColor(sf::Color(10, 12, 16, 200));
    r.setOutlineThickness(1.f);
    r.setOutlineColor(sf::Color(80, 90, 110, 180));
    return r;
}

static std::string modeTitle(Mode m) {
    if (m == Mode::SpinOnly) return "MODE 1: Spin only (textbook shortcut)";
    if (m == Mode::SpinAndMotion) return "MODE 2: Add motion (helicity appears)";
    return "MODE 3: Full conservation (orbital placeholder shown)";
}

int main() {
    sf::RenderWindow window(
        sf::VideoMode(sf::Vector2u{1100u, 700u}),
        sf::String("Beta Decay Viz (Learning Tool)"),
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setVerticalSyncEnabled(true);

    sf::Font font;
    bool hasFont = font.openFromFile("Arial.ttf") || font.openFromFile("DejaVuSans.ttf");

    const std::string TIP_NEUTRON_TITLE = "Neutron";
    const std::string TIP_NEUTRON_BODY =
        "This is the neutron before it breaks.\n\n"
        "Think of it like:\n"
        "  - One heavy ball\n"
        "  - Sitting still\n"
        "  - About to split\n\n"
        "It does nothing else here except exist as the starting point.\n"
        "It does not move because we are not teaching neutron motion,\n"
        "only what comes out of it.";

    const std::string TIP_PROTON_TITLE = "Proton";
    const std::string TIP_PROTON_BODY =
        "This is the proton after the break.\n\n"
        "Think:\n"
        "  - Neutron turns into a proton\n"
        "  - Proton is heavy\n"
        "  - So it barely moves\n\n"
        "In real life it can move, but we keep it still so it doesn't distract you.\n"
        "Red means: the heavy leftover.";

    const std::string TIP_ELECTRON_TITLE = "Electron (e-)";
    const std::string TIP_ELECTRON_BODY =
        "This is the electron.\n\n"
        "Think:\n"
        "  - A tiny piece that shoots out fast\n"
        "  - Light\n"
        "  - Easy to move\n\n"
        "The yellow glow just helps your eyes track it.";

    const std::string TIP_ANTINU_TITLE = "Anti-neutrino";
    const std::string TIP_ANTINU_BODY =
        "This is the anti-neutrino.\n\n"
        "Think:\n"
        "  - Even tinier than the electron\n"
        "  - Almost invisible in real life\n"
        "  - Flies off very fast\n\n"
        "It usually goes roughly the opposite way from the electron.";

    const std::string TIP_MOM_TITLE = "Momentum arrow";
    const std::string TIP_MOM_BODY =
        "This arrow means:\n"
        "\"Which way is this thing moving?\"";

    const std::string TIP_SPIN_TITLE = "Spin arrow";
    const std::string TIP_SPIN_BODY =
        "This arrow means:\n"
        "\"Which way is this thing spinning?\"\n\n"
        "This is the important one for the misconception.";

    const std::string TIP_SWIRL_TITLE = "Swirl (extra angular momentum)";
    const std::string TIP_SWIRL_BODY =
        "This swirl means:\n"
        "\"Something is missing if you only count spins.\" \n\n"
        "When the spins do not add up, motion must carry the extra turning.\n"
        "No swirl: spins alone work.\n"
        "Swirl: spins alone do not work.";


    std::mt19937 rng(static_cast<unsigned int>(std::random_device{}()));

    const sf::FloatRect arena(sf::Vector2f{60.f, 60.f}, sf::Vector2f{980.f, 580.f});
    sf::Vector2f origin(arena.position.x + 140.f, arena.position.y + arena.size.y * 0.5f);

    Mode mode = Mode::SpinOnly;
    bool paused = false;
    bool stepOnce = false;
    bool showHelp = true;

    float leftHandBias = 0.85f;
    DecayEvent current = makeEvent(rng, origin, leftHandBias, mode);

    sf::Clock clock;
    float t = 0.f;

    while (window.isOpen()) {
        float dtReal = clock.restart().asSeconds();
        float dt = dtReal;

        if (paused) {
            dt = 0.f;
            if (stepOnce) {
                dt = 1.f / 60.f;
                stepOnce = false;
            }
        }

        t += dt;

        while (auto ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) window.close();

            if (const auto* kp = ev->getIf<sf::Event::KeyPressed>()) {
                // Mode switches
                if (kp->code == sf::Keyboard::Key::Num1) {
                    mode = Mode::SpinOnly;
                    current = makeEvent(rng, origin, leftHandBias, mode);
                } else if (kp->code == sf::Keyboard::Key::Num2) {
                    mode = Mode::SpinAndMotion;
                    current = makeEvent(rng, origin, leftHandBias, mode);
                } else if (kp->code == sf::Keyboard::Key::Num3) {
                    mode = Mode::FullConservation;
                    current = makeEvent(rng, origin, leftHandBias, mode);
                }

                // Controls
                if (kp->code == sf::Keyboard::Key::Space) {
                    current = makeEvent(rng, origin, leftHandBias, mode);
                } else if (kp->code == sf::Keyboard::Key::Up) {
                    leftHandBias = std::min(0.99f, leftHandBias + 0.02f);
                    current = makeEvent(rng, origin, leftHandBias, mode);
                } else if (kp->code == sf::Keyboard::Key::Down) {
                    leftHandBias = std::max(0.01f, leftHandBias - 0.02f);
                    current = makeEvent(rng, origin, leftHandBias, mode);
                } else if (kp->code == sf::Keyboard::Key::P) {
                    paused = !paused;
                } else if (kp->code == sf::Keyboard::Key::N) {
                    if (paused) stepOnce = true;
                } else if (kp->code == sf::Keyboard::Key::H) {
                    showHelp = !showHelp;
                }
            }
        }

        Tooltip tip;
        sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        struct Seg { sf::Vector2f a; sf::Vector2f b; int kind; }; // kind 0 momentum, 1 spin
        std::vector<Seg> segs;

        // Update timing: only advance and auto-respawn when not paused
        if (dt > 0.f) {
            current.timeAlive += dt;
            if (current.timeAlive >= current.duration) {
                current = makeEvent(rng, origin, leftHandBias, mode);
            }
        }

        auto stepParticle = [&](Particle& p) {
            if (dt <= 0.f) return;

            p.pos += p.vel * dt;

            p.trailTimer += dt;
            if (p.trailTimer >= 0.02f) {
                p.trailTimer = 0.f;
                p.trail.push_back(p.pos);
                if (p.trail.size() > 70) p.trail.erase(p.trail.begin());
            }

            float left = arena.position.x;
            float top = arena.position.y;
            float right = arena.position.x + arena.size.x;
            float bottom = arena.position.y + arena.size.y;

            if (p.pos.x < left + p.radius) { p.pos.x = left + p.radius; p.vel.x *= -1.f; }
            if (p.pos.x > right - p.radius) { p.pos.x = right - p.radius; p.vel.x *= -1.f; }
            if (p.pos.y < top + p.radius) { p.pos.y = top + p.radius; p.vel.y *= -1.f; }
            if (p.pos.y > bottom - p.radius) { p.pos.y = bottom - p.radius; p.vel.y *= -1.f; }

            p.spinDir = vnorm(p.spinDir);
        };

        stepParticle(current.electron);
        stepParticle(current.antinu);

        // Evaluate the misconception claim
        // Claim: "the neutrino spins opposite the electron"
        // In this viz: use anti-nu. Opposite means spin vectors point opposite (dot < 0).
        float spinDot = vdot(vnorm(current.electron.spinDir), vnorm(current.antinu.spinDir));
        bool claimLooksTrue = (spinDot < -0.2f);

        // Helicity (only meaningful in modes 2 and 3)
        int hE = helicitySign(vnorm(current.electron.spinDir), vnorm(current.electron.vel));
        int hN = helicitySign(vnorm(current.antinu.spinDir), vnorm(current.antinu.vel));

        // Render
        window.clear(sf::Color(12, 14, 18));

        sf::RectangleShape box(arena.size);
        box.setPosition(arena.position);
        box.setFillColor(sf::Color(16, 18, 24));
        box.setOutlineThickness(2.f);
        box.setOutlineColor(sf::Color(70, 80, 95));
        window.draw(box);

        // neutron and proton
        drawGlowCircle(window, origin, 18.f, sf::Color(160, 210, 255));
        sf::Vector2f protonPos(origin.x + 40.f, origin.y);
        drawGlowCircle(window, protonPos, 14.f, sf::Color(255, 120, 150));
        if (hasFont) {
            drawLabel(window, font, origin + sf::Vector2f{0.f, -30.f}, "Neutron");
            drawLabel(window, font, protonPos + sf::Vector2f{0.f, -26.f}, "Proton");
        }


        // Orbital placeholder only in Mode 3
        if (mode == Mode::FullConservation) {
            drawOrbitalSwirl(window, origin, current.L_needed, t);
        }

        // Trails
        drawTrail(window, current.electron);
        drawTrail(window, current.antinu);

        // Particles
        drawGlowCircle(window, current.electron.pos, current.electron.radius, current.electron.color);
        drawGlowCircle(window, current.antinu.pos, current.antinu.radius, current.antinu.color);
        if (hasFont) {
            drawLabel(window, font, current.electron.pos + sf::Vector2f{0.f, -22.f}, "Electron");
            drawLabel(window, font, current.antinu.pos + sf::Vector2f{0.f, -22.f}, "Anti-neutrino");
        }


        auto drawVectors = [&](const Particle& p) {
            sf::Vector2f momDir = vnorm(p.vel);
            sf::Vector2f spinDir = vnorm(p.spinDir);

            if (mode == Mode::SpinOnly) {
                sf::Vector2f a = p.pos;
                sf::Vector2f b = p.pos + spinDir * 55.f;
                drawArrow(window, a, spinDir, 55.f, sf::Color(230, 230, 230, 220));
                segs.push_back(Seg{a, b, 1});
                return;
            }

            // momentum
            {
                sf::Vector2f a = p.pos;
                sf::Vector2f b = p.pos + momDir * 60.f;
                drawArrow(window, a, momDir, 60.f, sf::Color(150, 150, 150, 220));
                segs.push_back(Seg{a, b, 0});
            }

            // spin
            {
                sf::Vector2f off = vperp(momDir) * 10.f;
                sf::Vector2f a = p.pos + off;
                sf::Vector2f b = a + spinDir * 48.f;
                drawArrow(window, a, spinDir, 48.f, sf::Color(235, 235, 235, 220));
                segs.push_back(Seg{a, b, 1});
            }
        };

        drawVectors(current.electron);
        drawVectors(current.antinu);

        // HUD and teaching text
        if (hasFont) {
            // Top panel
            sf::Vector2f panelPos{arena.position.x + 10.f, arena.position.y + 10.f};
            sf::Vector2f panelSize{arena.size.x - 20.f, 140.f};
            auto panel = hudPanel(panelPos, panelSize);
            window.draw(panel);

            std::ostringstream ss;
            ss << modeTitle(mode) << (paused ? "   [PAUSED]" : "") << "\n";
            ss << "Keys: 1 2 3 modes   Space new decay   Up Down bias   P pause   N step   H help\n\n";

            ss << "Claim being tested: \"the neutrino spins opposite the electron\"\n";
            if (mode == Mode::SpinOnly) {
                ss << "Result: ALWAYS looks true here (by design). This mode is the oversimplified story.\n";
            } else {
                ss << "Result in this frame: " << (claimLooksTrue ? "looks true" : "does NOT look true") << " (spin dot = "
                   << std::fixed << std::setprecision(2) << spinDot << ")\n";
            }

            if (mode == Mode::SpinOnly) {
                ss << "What you are seeing: ONLY spin arrows. Motion is hidden, so the shortcut seems valid.\n";
            } else if (mode == Mode::SpinAndMotion) {
                ss << "What you are seeing: momentum (gray) and spin (white). Helicity depends on BOTH.\n";
            } else {
                ss << "What you are seeing: when spins do not balance, the swirl indicates extra angular momentum from motion.\n";
            }

            sf::Text text(font);
            text.setCharacterSize(16);
            text.setFillColor(sf::Color(230, 230, 230));
            text.setPosition(panelPos + sf::Vector2f{10.f, 8.f});
            text.setString(ss.str());
            window.draw(text);

            // Bottom panel: numeric readout only when it helps learning
            if (showHelp) {
                sf::Vector2f p2{arena.position.x + 10.f, arena.position.y + arena.size.y - 120.f};
                sf::Vector2f s2{arena.size.x - 20.f, 110.f};
                auto panel2 = hudPanel(p2, s2);
                window.draw(panel2);

                std::ostringstream s2s;
                s2s << "left bias: " << std::fixed << std::setprecision(2) << leftHandBias << "   proton spin sign: "
                    << (current.protonSpinSign > 0 ? "+1" : "-1") << "\n";

                if (mode == Mode::SpinOnly) {
                    s2s << "Mode 1 note: this forces opposite spins, so it cannot teach helicity or why the shortcut fails.\n";
                } else {
                    s2s << "electron helicity: " << (hE > 0 ? "+1" : "-1")
                        << "   anti nu helicity: " << (hN > 0 ? "+1" : "-1") << "\n";
                    s2s << "Helicity = sign(spin dot momentum). Flip motion and helicity can change.\n";
                }

                if (mode == Mode::FullConservation) {
                    if (current.L_needed == 0) {
                        s2s << "Conservation: spins alone balance (L_needed = 0).\n";
                    } else {
                        s2s << "Conservation: spins do NOT balance. Extra angular momentum must come from motion (L_needed = "
                            << current.L_needed << ").\n";
                    }
                } else {
                    s2s << "Tip: switch to Mode 3 to see why spin-only balancing is not generally sufficient.\n";
                }

                sf::Text text2(font);
                text2.setCharacterSize(16);
                text2.setFillColor(sf::Color(230, 230, 230));
                text2.setPosition(p2 + sf::Vector2f{10.f, 8.f});
                text2.setString(s2s.str());
                window.draw(text2);
            }
        }

        // Hover: dots
        if (hitCircle(mouse, origin, 24.f)) {
            tip.active = true;
            tip.title = TIP_NEUTRON_TITLE;
            tip.body = TIP_NEUTRON_BODY;
        } else if (hitCircle(mouse, protonPos, 20.f)) {
            tip.active = true;
            tip.title = TIP_PROTON_TITLE;
            tip.body = TIP_PROTON_BODY;
        } else if (hitCircle(mouse, current.electron.pos, 18.f)) {
            tip.active = true;
            tip.title = TIP_ELECTRON_TITLE;
            tip.body = TIP_ELECTRON_BODY;
        } else if (hitCircle(mouse, current.antinu.pos, 16.f)) {
            tip.active = true;
            tip.title = TIP_ANTINU_TITLE;
            tip.body = TIP_ANTINU_BODY;
        }

        // Hover: swirl (Mode 3 only)
        if (!tip.active && mode == Mode::FullConservation) {
            // Treat swirl as a ring around origin: detect near radius band
            float d = vlen(mouse - origin);
            float targetR = 22.f + std::abs(current.L_needed) * 10.f;
            if (std::abs(d - targetR) < 14.f) {
                tip.active = true;
                tip.title = TIP_SWIRL_TITLE;
                tip.body = TIP_SWIRL_BODY;
            }
        }

        // Hover: arrows
        if (!tip.active) {
            for (const auto& s : segs) {
                float d = pointSegmentDistance(mouse, s.a, s.b);
                if (d < 8.f) {
                    tip.active = true;
                    if (s.kind == 0) {
                        tip.title = TIP_MOM_TITLE;
                        tip.body = TIP_MOM_BODY;
                    } else {
                        tip.title = TIP_SPIN_TITLE;
                        tip.body = TIP_SPIN_BODY;
                    }
                    break;
                }
            }
        }

        // Draw tooltip last (on top of everything)
        if (hasFont && tip.active) {
            drawTooltipBox(window, font, mouse, tip.title, tip.body);
        }

        window.display();
    }

    return 0;
}
