// Per-model text. The parameters, their ranges and their help lines all come out
// of the C — there is exactly one place in this project where a range is written
// down, and it is the ParamDef table in the model's own source file. What lives
// here is the part a compiler cannot hold: what the model is, where it came
// from, and what to go and look at.
//
// The `try` recipes are the most important thing on this page. A five-parameter
// model has a five-dimensional space in it, almost all of which is boring, and
// nobody finds the interesting corner by waggling sliders at random. So we say
// where to go and what will happen when you get there.

export const INFO = {
  colony: {
    tagline: 'How a colony finds food, and what it becomes when there is not enough.',
    what: `A drop of bacteria in the middle of an agar plate. The blue is nutrient; the
      black is the colony. Nothing here knows what shape it is supposed to be. Each cell
      eats what is under it, pays a small metabolic cost every step, divides when it has
      banked enough, and starves into a hard spore when it cannot — and what you are
      looking at is mostly the spores, because a colony is a scaffold of the spent with
      a thin living rind at its surface. The shape is a by-product.`,
    how: `Bacteria eat, so a colony digs a depletion halo around itself, and the only
      food left is beyond that halo. Now consider a bump on the edge of the colony. It
      pokes further out into fresh nutrient than the flat parts do, so it eats better,
      so it grows faster, so it becomes a bigger bump. That runaway is the
      Mullins-Sekerka instability, and it is the same mathematics as a snowflake, a
      lightning strike, and oil fingering through water.
      \n\nStarve the colony and the instability wins: the front cannot advance
      everywhere at once, so it advances in fingers, and you get a fractal. Feed it
      well and diffusion refills the halo as fast as the cells empty it, no bump gets
      an advantage over any other, and you get a disc.
      \n\nWatch the D readout while you do it. That is the box-counting dimension of
      the colony, measured live, and it is the number the whole model is answerable to.`,
    try: [
      {
        label: 'starve it',
        note: `n0 = 0.10, and give it a few thousand generations. The colony branches
          into a dendrite and D falls to about 1.6 — heading for 1.71, which is the
          dimension Witten and Sander derived for diffusion-limited aggregation in 1981
          from an argument about random walkers that has nothing to do with biology,
          and which Fujikawa and Matsushita then measured on an actual plate of
          Bacillus subtilis in 1989. The same number, out of a physics paper and out of
          a Petri dish.`,
        params: { n0: 0.10, chiN: 0, chiR: 0 },
      },
      {
        label: 'feed it',
        note: `n0 = 0.45. The branches fill in and the colony becomes a compact disc.
          D goes to 2.0. Nothing about the cells changed. You changed how much food was
          in the dish, and the colony changed what kind of thing it is.`,
        params: { n0: 0.45, chiN: 0, chiR: 0 },
      },
      {
        label: 'kill the instability',
        note: `Keep the food low but drop substeps to 1 and Dn to 0.04. Now the nutrient
          barely travels between one division and the next, so the front only knows
          about the food immediately in front of it, no tip can get ahead of its
          neighbours, and the branching stops — even though the colony is still
          starving. Branching is not caused by hunger. It is caused by hunger plus a
          long diffusion length.`,
        params: { n0: 0.12, Dn: 0.04, substeps: 1 },
      },
      {
        label: 'let them talk',
        note: `Wind chiR up to 20 with gammaR at 0.5. Starving cells emit a
          chemorepellent and other cells grow away from it, so the branches push each
          other apart instead of tangling. The cells are not cooperating; they are
          avoiding each other's distress signals, and organisation is what falls out.
          This is Ben-Jacob's result, and it is why he called them communicating
          walkers.`,
        params: { n0: 0.15, chiR: 20, gammaR: 0.5, chiN: 4 },
      },
      {
        label: 'chase the food',
        note: `chiN = 25. Now the cells climb the nutrient gradient instead of stumbling
          into it. The tips sharpen and drive outward, and the colony gets to the food
          faster — and, because it no longer has to branch to find it, ends up denser.`,
        params: { n0: 0.15, chiN: 25, chiR: 0 },
      },
    ],
    refs: [
      'Ben-Jacob, E., Schochet, O., Tenenbaum, A., Cohen, I., Czirók, A. & Vicsek, T. "Generic modelling of cooperative growth patterns in bacterial colonies." Nature 368, 46–49 (1994).',
      'Fujikawa, H. & Matsushita, M. "Fractal growth of Bacillus subtilis on agar plates." J. Phys. Soc. Jpn. 58, 3875–3878 (1989).',
      'Matsushita, M. et al. "Interface growth and pattern formation in bacterial colonies." Physica A 249, 517–524 (1998).',
      'Witten, T. A. & Sander, L. M. "Diffusion-limited aggregation, a kinetic critical phenomenon." Phys. Rev. Lett. 47, 1400–1403 (1981).',
    ],
  },

  grayscott: {
    tagline: 'Two chemicals, no biology, and something that divides like a cell.',
    what: `U is a food chemical, fed into the dish everywhere at a constant rate. V eats
      U and turns it into more V. V also decays away. That is the whole system: two
      numbers per point in space, and no notion of a cell, an organism, or a rule.`,
    how: `du/dt = Du·∇²u − uv² + F(1−u)
      \ndv/dt = Dv·∇²v + uv² − (F+k)v
      \n\nV is autocatalytic — it needs itself in order to be made — so it can only get
      going where there is already some V, and it burns the U around it doing so. U
      diffuses twice as fast as V, and that difference is everything: it is Turing's
      condition, local activation with long-range inhibition, and it is why a leopard
      has spots.
      \n\nPearson mapped the (F, k) plane in 1993 and found a dozen qualitatively
      different regimes in it. Two numbers.`,
    try: [
      {
        label: 'mitosis',
        note: `F = 0.026, k = 0.061. A spot grows, stretches, pinches in the middle, and
          becomes two spots. Which then do it again. It is cell division, performed by a
          differential equation that has never heard of a cell, and it is the single
          most unsettling image in this laboratory.`,
        params: { F: 0.026, k: 0.061 },
      },
      {
        label: 'coral',
        note: 'F = 0.0545, k = 0.062. Branching growth that stops when it runs into itself.',
        params: { F: 0.0545, k: 0.062 },
      },
      {
        label: 'worms',
        note: 'F = 0.058, k = 0.065. Stripes that grow from their ends and never quite settle.',
        params: { F: 0.058, k: 0.065 },
      },
      {
        label: 'gliders',
        note: `F = 0.014, k = 0.054 — Munafo's U-Skate World. Localised blobs that
          propel themselves across the dish, collide, and annihilate. Reaction-diffusion
          has spaceships.`,
        params: { F: 0.014, k: 0.054 },
      },
      {
        label: 'maze',
        note: 'F = 0.029, k = 0.057. A labyrinth assembles itself and then holds still.',
        params: { F: 0.029, k: 0.057 },
      },
    ],
    refs: [
      'Pearson, J. E. "Complex patterns in a simple system." Science 261, 189–192 (1993).',
      'Gray, P. & Scott, S. K. "Autocatalytic reactions in the isothermal, continuous stirred tank reactor." Chem. Eng. Sci. 38, 29–43 (1983).',
      'Turing, A. M. "The chemical basis of morphogenesis." Phil. Trans. R. Soc. B 237, 37–72 (1952).',
      'Munafo, R. "Reaction-Diffusion by the Gray-Scott Model: Pearson\'s Parametrization." mrob.com/pub/comp/xmorphia',
    ],
  },

  lenia: {
    tagline: 'Conway\'s Life with every discrete thing about it dissolved. Something survives.',
    what: `The creature swimming across the screen is Orbium unicaudatus. It is not a
      picture of an organism; it is a region of a continuous field that holds itself
      together, moves under its own power, recovers when you damage it, and dies if you
      change the conditions it lives in.`,
    how: `Take Life. Make the cell's state a real number instead of a bit. Make the
      neighbourhood a smooth ring-shaped kernel of radius R instead of the eight
      adjacent cells, so the "neighbour count" becomes a convolution. Make the birth and
      survival sets a smooth bump — grow if the neighbourhood sum is near μ, shrink
      otherwise — instead of the sets {3} and {2,3}. Make the time step small instead
      of one.
      \n\nA ← clip( A + G(K∗A)/T , 0, 1 ),   G(u) = 2·exp(−(u−μ)²/2σ²) − 1
      \n\nEvery discrete choice is gone, and the gliders survive the dissolution — but
      they are no longer gliders. They do not travel along lattice directions at
      rational speeds, because there is no lattice left to travel along. They swim.`,
    try: [
      {
        label: 'find the edge',
        note: `Take σ and move it, slowly, in either direction from 0.015. Below about
          0.012 Orbium starves and evaporates. Above about 0.018 it blooms into
          featureless mush that fills the world. Life occupies a filament between
          extinction and cancer, and it is about six thousandths wide. This is the
          "edge of chaos", and unlike most demonstrations of it, you do not have to take
          anybody's word for it — you can find both edges yourself in ten seconds.`,
        params: { mu: 0.15, sigma: 0.015 },
      },
      {
        label: 'damage it',
        note: `While it is swimming, hold the right mouse button and erase a piece out of
          it. If you did not take too much, it will pull itself back together and carry
          on. Nothing in the rule says "repair".`,
        params: {},
      },
      {
        label: 'primordial soup',
        note: `Set init to noise and watch. Most of the time it dies or it floods.
          Occasionally something crawls out.`,
        params: { init: 1, mu: 0.15, sigma: 0.017 },
      },
      {
        label: 'the wrong physics',
        note: `Switch core to exp. This is the kernel the paper writes down — and
          Orbium, which was found under the polynomial one, immediately stops working.
          The creature is not a shape. It is a shape that fits a particular set of laws.`,
        params: { core: 1 },
      },
    ],
    refs: [
      'Chan, B. W.-C. "Lenia — Biology of Artificial Life." Complex Systems 28(3), 251–286 (2019). arXiv:1812.05433.',
      'Chan, B. W.-C. "Lenia and Expanded Universe." ALIFE 2020. arXiv:2005.03742.',
      'Rafler, S. "Generalization of Conway\'s Game of Life to a continuous domain — SmoothLife." arXiv:1111.1567 (2011).',
    ],
  },

  rps: {
    tagline: 'Three species, none of them best. Whether they all survive is decided by one number.',
    what: `Rock beats scissors, scissors beats paper, paper beats rock. Three species in
      a cycle where nobody is strongest. In a well-mixed flask this ends badly: the
      populations swing harder and harder until one hits zero, and then a second
      follows, and you are left with a monoculture.`,
    how: `On a lattice, where organisms only interact with their neighbours, something
      else happens — and exactly one parameter decides which.
      \n\nTurn mobility (eps) down and the system organises itself into interlocking
      spiral waves, each species eternally chasing the one it beats and fleeing the one
      that beats it. All three persist indefinitely.
      \n\nTurn mobility up and the spiral arms get fatter — their wavelength grows as
      the square root of the mobility. Push it far enough and the spirals no longer fit
      in the world. The pattern washes out, and two species go extinct.
      \n\nSo biodiversity here is not a property of the species, or of who beats whom. It
      is a property of how far things wander.`,
    try: [
      {
        label: 'spirals',
        note: 'eps = 2. Give it a few hundred generations and the spirals assemble themselves out of noise.',
        params: { eps: 2 },
      },
      {
        label: 'kill the biodiversity',
        note: `Wind eps up to 50 and wait. The spirals grow until they outgrow the
          lattice, and then the ecosystem collapses to a single species. One slider,
          from a living ecosystem to a monoculture.`,
        params: { eps: 50 },
      },
      {
        label: 'watch the transition',
        note: `Creep eps up from 5 in small steps and watch the spiral arms thicken.
          Somewhere around eps ≈ 15 on this lattice the arms get comparable to the box
          and coexistence starts failing. This is a phase transition, and it is visible.`,
        params: { eps: 12 },
      },
    ],
    refs: [
      'Reichenbach, T., Mobilia, M. & Frey, E. "Mobility promotes and jeopardizes biodiversity in rock–paper–scissors games." Nature 448, 1046–1049 (2007).',
      'Kerr, B., Riley, M. A., Feldman, M. W. & Bohannan, B. J. M. "Local dispersal promotes biodiversity in a real-life game of rock–paper–scissors." Nature 418, 171–174 (2002).',
    ],
  },

  sugarscape: {
    tagline: 'A perfectly fair world, and the wealth distribution it produces.',
    what: `Two mountains of sugar (blue) and a population of agents (black) living off
      them. Each agent has a vision, a metabolism, and a starting purse, all drawn at
      random from the same distributions. Each tick it looks as far as it can see along
      the four compass directions, moves to the richest unoccupied square it can find,
      eats everything there, and pays its metabolism. If it ever runs out, it starves.`,
    how: `The rules are scrupulously fair. Nobody inherits, nobody cheats, nobody is
      taxed, nobody is favoured, and the endowments are all drawn from one uniform
      distribution.
      \n\nWatch the Gini coefficient. It starts near zero, because everyone begins with
      roughly the same. It climbs, and it keeps climbing, and it settles around 0.4–0.5,
      which is about where a real economy sits. A small number of agents end up very
      rich and a long tail end up with nothing, and no rule anywhere in the model says
      that should happen.
      \n\nAlso watch the population, which nobody set. Start it at 400 or at 800; either
      way it converges on the same number, the carrying capacity of the landscape. That
      number is a property of the sugar and the metabolisms, not of anything anyone
      decided.`,
    try: [
      {
        label: 'inequality',
        note: `Just let it run and watch the Gini climb. Then look at the Lorenz curve
          under the plots: the further it bows away from the diagonal, the more unequal
          the world. It bows.`,
        params: { growback: 1 },
      },
      {
        label: 'famine',
        note: `Drop growback to 0.1. The carrying capacity collapses, the population
          crashes to a fraction of what it was, and the survivors cluster on the peaks.
          Scarcity does not distribute itself evenly.`,
        params: { growback: 0.1 },
      },
      {
        label: 'abundance',
        note: `growback = 4. There is enough for everyone, everywhere, and inequality
          drops — but it does not vanish. Vision still pays.`,
        params: { growback: 4 },
      },
    ],
    refs: [
      'Epstein, J. M. & Axtell, R. Growing Artificial Societies: Social Science from the Bottom Up. Brookings Institution Press / MIT Press (1996).',
    ],
  },

  schelling: {
    tagline: 'Nobody here is a segregationist. The city segregates anyway.',
    what: `Two kinds of household, and some empty houses. Every household looks at its
      eight neighbours and asks one question: what fraction of them are like me? If that
      fraction is below its threshold tau, it moves somewhere it would be happier.`,
    how: `Set tau to 0.35. You have now written down a population in which every single
      household is perfectly content to be outnumbered nearly two to one — more tolerant
      than any society that has ever existed. Run it. The city segregates, and the
      segregation index goes from 0.5 (which is what pure chance gives you) to about
      0.77.
      \n\nThe mechanism is worth having clearly. A household that moves because it was
      slightly uncomfortable leaves its old street slightly more uniform for the
      neighbours it left behind, and makes its new street slightly more mixed for the
      ones it lands among — some of whom now become uncomfortable, and move in turn. The
      system ratchets. Mild individual preferences compose into a collective outcome
      that nobody chose and nobody wanted.
      \n\nSchelling's conclusion, in 1971, was that you cannot read the preferences of
      individuals off the pattern of the aggregate. He got a Nobel prize for it, and it
      is the most useful thing this laboratory has to say about people.`,
    try: [
      {
        label: 'the tolerant city',
        note: 'tau = 0.15. Households happy with almost any mix. The city stays mixed. It can be done.',
        params: { tau: 0.15 },
      },
      {
        label: 'mild preference',
        note: 'tau = 0.35. Everyone is content in a two-to-one minority. The city divides anyway.',
        params: { tau: 0.35 },
      },
      {
        label: 'gridlock',
        note: `tau = 0.8. Now nobody can be satisfied — there is no arrangement of the
          city in which everyone is happy — and the households churn forever, moving
          endlessly and never settling.`,
        params: { tau: 0.8 },
      },
    ],
    refs: [
      'Schelling, T. C. "Dynamic models of segregation." Journal of Mathematical Sociology 1(2), 143–186 (1971).',
      'Schelling, T. C. Micromotives and Macrobehavior. Norton (1978).',
    ],
  },

  life: {
    tagline: 'Not the Game of Life. All 262,144 of them.',
    what: `A cell counts its eight neighbours. If it is dead it is born when that count
      is in the birth set B; if it is alive it survives when the count is in the survival
      set S. Conway's rule is B3/S23. There are 2^18 such rules, and this one kernel runs
      every one of them.`,
    how: `next = (alive ? S : B) >> n & 1
      \n\nB and S are just two nine-bit masks, so the rule is two integers and changing
      it costs nothing. Which means the thing to explore here is not Life. It is the
      space Life sits in.`,
    try: [
      {
        label: 'Conway (B3/S23)',
        note: 'Gliders, still lifes, glider guns. Universal: you can build a computer in it.',
        params: { B: 8, S: 12 },
      },
      {
        label: 'HighLife (B36/S23)',
        note: `One digit different from Conway, and it contains a replicator — a small
          pattern that builds copies of itself. Self-reproduction, from an accident of
          notation.`,
        params: { B: 72, S: 12 },
      },
      {
        label: 'Seeds (B2/S)',
        note: 'Nothing ever survives. Every cell dies every generation. The result is not death; it is an explosion.',
        params: { B: 4, S: 0, density: 0.02 },
      },
      {
        label: 'Anneal (B4678/S35678)',
        note: `Domains coarsen, boundaries shorten, and the pattern behaves exactly as if
          it had surface tension. It does not have surface tension. It has a rule.`,
        params: { B: 464, S: 488, density: 0.5 },
      },
      {
        label: 'Coral (B3/S45678)',
        note: 'Slow dendritic growth. A preview of the colony model, with no chemistry in it at all.',
        params: { B: 8, S: 496, density: 0.2 },
      },
    ],
    refs: [
      'Gardner, M. "Mathematical Games: The fantastic combinations of John Conway\'s new solitaire game \'life\'." Scientific American 223(4), 120–123 (1970).',
      'Berlekamp, E. R., Conway, J. H. & Guy, R. K. Winning Ways for Your Mathematical Plays, Vol. 2. Academic Press (1982).',
    ],
  },

  eca: {
    tagline: 'Eight bits of rule. One of them is a universal computer.',
    what: `A line of cells, each 0 or 1. Each looks at itself and its two neighbours —
      eight possible patterns — and the rule is nothing but a choice of output bit for
      each of the eight. Eight bits: 256 rules. Time runs downward, so what you see is
      the entire history of the line.`,
    how: `next = (rule >> (left·4 + self·2 + right)) & 1
      \n\nThat is the whole thing. Wolfram sorted the 256 into four classes: those that
      die, those that freeze, those that boil, and the rare fourth kind that do neither.
      \n\nThe entropyVar readout classifies the rule for you, using Wuensche's method
      rather than by eye. Count how many cells matched each of the eight neighbourhood
      patterns this step, and take the entropy of that histogram. Rules that freeze drive
      it low and flat. Rules that boil hold it high and flat. The interesting rules sit
      in between with a large variance, because the entropy jerks around as structures
      form, collide, and die. The variance of the input entropy is a glider detector.`,
    try: [
      {
        label: 'rule 90 — a fractal',
        note: `Rule 90 is left XOR right. Start it from a single live cell and it draws
          the Sierpinski triangle. A fractal, from a rule you can hold in your head.`,
        params: { rule: 90, init: 0 },
      },
      {
        label: 'rule 30 — a chaos',
        note: `From one live cell. The left side is orderly and the middle is not.
          Mathematica used the centre column of rule 30 as its random number generator
          for years.`,
        params: { rule: 30, init: 0 },
      },
      {
        label: 'rule 110 — a computer',
        note: `Start from a random line and look for the gliders travelling through the
          striped background. Matthew Cook proved in 2004 that collisions between them
          can be arranged to perform any computation whatsoever. This rule is Turing
          complete. It is eight bits long.`,
        params: { rule: 110, init: 1, density: 0.5 },
      },
      {
        label: 'rule 184 — a traffic jam',
        note: `Cars move right into empty space. Below 50% density traffic flows; above
          it, jams form and persist. Set density to 0.45, then to 0.55, and watch the
          phase transition.`,
        params: { rule: 184, init: 1, density: 0.55 },
      },
    ],
    refs: [
      'Wolfram, S. "Statistical mechanics of cellular automata." Rev. Mod. Phys. 55, 601–644 (1983).',
      'Cook, M. "Universality in Elementary Cellular Automata." Complex Systems 15, 1–40 (2004).',
      'Wuensche, A. "Classifying cellular automata automatically." Complexity 4(3), 47–66 (1999).',
    ],
  },

  ant: {
    tagline: 'Ten thousand steps of chaos, and then, for no reason, a road.',
    what: `An ant on a grid. On a white square it turns right, flips the square black,
      and steps forward. On a black square it turns left, flips it white, and steps
      forward. That is the complete rule. There are no parameters.`,
    how: `Three things happen, in order, and the third one is why this is here.
      \n\nFor a few hundred steps the trail is small and roughly symmetric. Then, for
      about ten thousand steps, it is a mess — a growing chaotic blot, and if you did not
      know better you would say the ant was drawing noise forever.
      \n\nAnd then it stops. Without anything changing, without any parameter being
      tuned, the ant falls into a cycle of 104 steps that carries it two squares
      diagonally, and it builds a perfectly straight road, and it builds that road
      forever.
      \n\nNobody put the road in the rule. Nobody can look at the rule and predict it.
      The only way to find out that it is there is to run the ant.`,
    try: [
      {
        label: 'wait for the highway',
        note: `Turn speed up to 200 and watch the generation counter. Somewhere around
          step 10,000 the chaos will stop and the ant will start building. It is worth
          seeing once.`,
        params: { rule: 0, ants: 1, stride: 200 },
      },
      {
        label: 'two ants',
        note: `Two ants each build their own highway — until one drives across the
          other's road, and both of them fall back into chaos.`,
        params: { rule: 0, ants: 2, stride: 120 },
      },
      {
        label: 'a turmite',
        note: 'LLRR: more colours, more turns. It grows a symmetric, almost decorative form instead.',
        params: { rule: 2, ants: 1, stride: 120 },
      },
    ],
    refs: [
      'Langton, C. G. "Studying artificial life with cellular automata." Physica D 22, 120–149 (1986).',
      'Bunimovich, L. A. & Troubetzkoy, S. E. "Recurrence properties of Lorentz lattice gas cellular automata." J. Stat. Phys. 67, 289–302 (1992).',
    ],
  },

  forestfire: {
    tagline: 'Why the world is full of power laws when criticality is supposed to be a knife-edge.',
    what: `Trees grow at random with probability p. Lightning strikes with probability f.
      Fire spreads to neighbouring trees and leaves bare ground behind.`,
    how: `The interesting quantity is neither p nor f but the ratio between them.
      \n\nMake lightning very rare compared with growth and the forest does something
      nobody told it to do: it fills up until it is just on the edge of percolating, and
      it stays there. Fires then come in every size — mostly tiny, occasionally
      middling, and every so often one that takes the entire continent. The distribution
      of fire sizes is a power law: there is no typical fire.
      \n\nNothing was tuned to make that happen. In ordinary critical phenomena you have
      to hold the temperature at exactly T_c, and an experimenter has to work to keep it
      there. Here the system drives itself to its own critical point and sits on it. That
      is self-organised criticality, and it is Bak, Tang and Wiesenfeld's answer to a
      genuinely deep question: why is the natural world so full of power laws —
      earthquakes, avalanches, extinctions, market crashes — when criticality ought to be
      a knife-edge that nothing should be balanced on?`,
    try: [
      {
        label: 'criticality',
        note: `p = 0.02, f = 0.00002 — a ratio of a thousand to one. Let it run for a
          few thousand generations and watch the fire-size histogram fill in as a
          straight line on log-log axes. That straight line is the power law.`,
        params: { p: 0.02, f: 0.00002 },
      },
      {
        label: 'destroy the power law',
        note: `Wind f up to 0.001. Lightning is now common, the forest never gets dense
          enough to carry a big fire, every fire is small, and the distribution acquires
          a scale. The scale-free behaviour was never a property of the rules. It was a
          property of the rules plus the separation of timescales.`,
        params: { p: 0.02, f: 0.001 },
      },
      {
        label: 'light one yourself',
        note: 'Drag on the canvas to set fire to the forest, and watch how far it gets.',
        params: {},
      },
    ],
    refs: [
      'Drossel, B. & Schwabl, F. "Self-organized critical forest-fire model." Phys. Rev. Lett. 69, 1629–1632 (1992).',
      'Bak, P., Tang, C. & Wiesenfeld, K. "Self-organized criticality: An explanation of 1/f noise." Phys. Rev. Lett. 59, 381–384 (1987).',
      'Clauset, A., Shalizi, C. R. & Newman, M. E. J. "Power-law distributions in empirical data." SIAM Review 51, 661–703 (2009).',
    ],
  },
};
