void reverseCallLocator(Function &F){
   typedef std::reverse_iterator <Function::iterator> revFun;
   for (revFun I = revFun(F.end()), E = revFun(F.begin()); I != E; I++) {
        typedef std::reverse_iterator<BasicBlock::iterator> rev;
        for (rev II = rev(I->end()), EE = rev(I->begin()); II != EE; ++II) {
                const Instruction *inst = &*II;
                if(inst->mayWriteToMemory())
                   errs() << *I << "\n";
        }
   }
}


/*namespace {
   class Slicer : public ModulePass {
     public:
        static char ID;
        Slicer() : ModulePAss(ID) {}

        bool Slicer::runOnModule(Module &M){
           errs() << "beginning module slice\n";

        }
        void getAnalysisUsage(AnalysisUsage &AU) const {
           AU.addRequired<PostDominatorTree>();
           AU.addRequired<PostDominanceFrontier>();
      }
   };
}


static RegisterPass<Slicer> X("slice", "Slices the code");
char Slicer::ID = 0;*/

