import React from "react";
import { Composition } from "remotion";
import { FPS } from "./config";
import { Apresentacao, DURACAO_TOTAL } from "./Video";

export const Root: React.FC = () => (
  <Composition id="Apresentacao" component={Apresentacao} durationInFrames={DURACAO_TOTAL} fps={FPS} width={1920} height={1080} />
);
