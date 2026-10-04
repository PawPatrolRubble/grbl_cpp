#pragma once
#include "plotting/PlotJobBuilder.h"
#include "grbl/ToolLibrary.h"
#include "grbl/GraphicsDesign/Document.h"
#include "grbl/GraphicsDesign/LineEntity.h"

struct AutomaticRackFixture {
    ToolLibrary library;
    Document document;
    QUuid black = library.addTool("Black", Qt::black), red = library.addTool("Red", Qt::red);
    PlotOptions options;
    QList<QUuid> paths;
    AutomaticRackFixture() {
        PenRackSettings rack;
        rack.configured = true;
        library.updateRackSettings(rack);
        rack.motionCalibrated = true; library.updateRackSettings(rack);
        int slot = 0;
        for (const auto& id : {black, red}) {
            auto profile = library.penRackStation(id);
            profile.configured = true; profile.slot = QString::number(++slot);
            profile.slotX = -20 * slot; profile.slotY = -20;
            profile.returnMacro = "G53 G1 X{slotX} F{dockFeed}";
            profile.pickupMacro = profile.returnMacro;
            library.updatePenRackStation(id, profile);
            profile.motionCalibrated = true; library.updatePenRackStation(id, profile);
        }
        for (const auto& pen : {black, black, red, red, black}) {
            auto path = std::make_unique<LineEntity>(QPointF(10, 10), QPointF(20, 20));
            path->setToolId(pen); paths << path->id(); document.addEntity(std::move(path));
        }
        options.automaticRack = true; options.initialHolderKnown = true;
    }
    PlotJob job() const { return PlotJobBuilder::build(document, options, &library).job; }
};
