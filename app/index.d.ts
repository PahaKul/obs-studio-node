// all global interfaces here

interface Dictionary<TItemType> {
  [key: string]: TItemType;
}

interface IRectangle {
  x: number;
  y: number;
  width: number;
  height: number;
}

interface IScalableRectangle extends IRectangle {
  scaleX?: number;
  scaleY?: number;
}

declare type TPatch<TEntity> = { id: string } & Partial<TEntity>;

interface IVec2 {
  x: number;
  y: number;
}

// allow TS to import components without type definitions
// webpack still check the module existence
declare module '*';