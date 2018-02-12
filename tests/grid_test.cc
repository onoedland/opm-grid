#include <config.h>

// Warning suppression for Dune includes.
#include <opm/grid/utility/platform_dependent/disable_warnings.h>

#include <dune/common/unused.hh>
#include <dune/grid/CpGrid.hpp>
#include <dune/grid/polyhedralgrid.hh>
#include <opm/grid/cpgrid/GridHelpers.hpp>
#include <dune/grid/io/file/vtk/vtkwriter.hh>

#include <opm/grid/cpgrid/dgfparser.hh>
#include <dune/grid/polyhedralgrid/dgfparser.hh>

#define DISABLE_DEPRECATED_METHOD_CHECK 1
#if DUNE_VERSION_NEWER(DUNE_GRID,2,5)
#include <dune/grid/test/gridcheck.hh>
#endif

// Re-enable warnings.
#include <opm/grid/utility/platform_dependent/reenable_warnings.h>

#include <opm/grid/utility/OpmParserIncludes.hpp>

#include <iostream>

const char *deckString =
    "RUNSPEC\n"
    "METRIC\n"
    "DIMENS\n"
    "2 2 2 /\n"
    "GRID\n"
    "DXV\n"
    "2*1 /\n"
    "DYV\n"
    "2*1 /\n"
    "DZ\n"
    "8*1 /\n"
    "TOPS\n"
    "4*100.0 /\n";

template <class GridView>
void testGridIteration( const GridView& gridView )
{
    typedef typename GridView::template Codim<0>::Iterator ElemIterator;
    typedef typename GridView::IntersectionIterator IsIt;
    typedef typename GridView::template Codim<0>::Geometry Geometry;
    typedef typename GridView::Grid::LocalIdSet LocalIdSet;

    const LocalIdSet& localIdSet = gridView.grid().localIdSet();

    int numElem = 0;
    ElemIterator elemIt = gridView.template begin<0>();
    ElemIterator elemEndIt = gridView.template end<0>();
    for (; elemIt != elemEndIt; ++elemIt) {
        const Geometry& elemGeom = elemIt->geometry();
        if (std::abs(elemGeom.volume() - 1.0) > 1e-8)
            std::cout << "element's " << numElem << " volume is wrong:"<<elemGeom.volume()<<"\n";

        typename Geometry::LocalCoordinate local( 0.5 );
        typename Geometry::GlobalCoordinate global = elemGeom.global( local );
        typename Geometry::GlobalCoordinate center = elemGeom.center();
        if( (center - global).two_norm() > 1e-6 )
        {
          std::cout << "center = " << center << " global( localCenter ) = " << global << std::endl;
        }


        int numIs = 0;
        IsIt isIt = gridView.ibegin(*elemIt);
        IsIt isEndIt = gridView.iend(*elemIt);
        for (; isIt != isEndIt; ++isIt, ++ numIs)
        {
            const auto& intersection = *isIt;
            const auto& isGeom = intersection.geometry();
            std::cout << "Checking intersection id = " << localIdSet.id( intersection ) << std::endl;
            if (std::abs(isGeom.volume() - 1.0) > 1e-8)
                std::cout << "volume of intersection " << numIs << " of element " << numElem << " volume is wrong: " << isGeom.volume() << "\n";

            if (intersection.neighbor())
            {
              if( numIs != intersection.indexInInside() )
                  std::cout << "num iit = " << numIs << " indexInInside " << intersection.indexInInside() << std::endl;
#if DUNE_VERSION_NEWER(DUNE_GRID,2,4)
              if (std::abs(intersection.outside().geometry().volume() - 1.0) > 1e-8)
                  std::cout << "outside element volume of intersection " << numIs << " of element " << numElem
                            << " volume is wrong: " << intersection.outside().geometry().volume() << std::endl;

              if (std::abs(intersection.inside().geometry().volume() - 1.0) > 1e-8)
                  std::cout << "inside element volume of intersection " << numIs << " of element " << numElem
                            << " volume is wrong: " << intersection.inside().geometry().volume() << std::endl;
#endif
            }
        }

        if (numIs != 6)
            std::cout << "number of intersections is wrong for element " << numElem << "\n";

        ++ numElem;
    }

    if (numElem != 2*2*2)
        std::cout << "number of elements is wrong: " << numElem << "\n";
}

template <class Grid>
void testGrid(Grid& grid, const std::string& name)
{
    typedef typename Grid::LeafGridView GridView;
#if DUNE_VERSION_NEWER(DUNE_GRID,2,5)
    try {
      gridcheck( grid );
    }
    catch ( const Dune::Exception& e)
    {
      std::cerr << "Warning: " << e.what() << std::endl;
    }
#endif

    testGridIteration( grid.leafGridView() );

    std::cout << "create vertex mapper\n";
    Dune::MultipleCodimMultipleGeomTypeMapper<GridView,
                                              Dune::MCMGVertexLayout> mapper(grid.leafGridView());

    std::cout << "VertexMapper.size(): " << mapper.size() << "\n";
    if (mapper.size() != 27) {
        std::cout << "Wrong size of vertex mapper. Expected 27!\n";
        std::abort();
    }

    // VTKWriter does not work with geometry type none at the moment
    if( grid.geomTypes( 0 )[ 0 ].isCube() )
    {
      std::cout << "create vtkWriter\n";
      typedef Dune::VTKWriter<GridView> VtkWriter;
      VtkWriter vtkWriter(grid.leafGridView());

      std::cout << "create cellData\n";
      int numElems = grid.size(0);
      std::vector<double> tmpData(numElems, 0.0);

      std::cout << "add cellData\n";
      vtkWriter.addCellData(tmpData, name);

      std::cout << "write data\n";
      vtkWriter.write(name, Dune::VTK::ascii);
    }

}

int main(int argc, char** argv )
{
    // initialize MPI
    Dune::MPIHelper::instance( argc, argv );

    std::stringstream dgfFile;
    // create unit cube with 8 cells in each direction
    dgfFile << "DGF" << std::endl;
    dgfFile << "Interval" << std::endl;
    dgfFile << "0 0 0" << std::endl;
    dgfFile << "1 1 1" << std::endl;
    dgfFile << "8 8 8" << std::endl;
    dgfFile << "#" << std::endl;

#if HAVE_OPM_PARSER
    Opm::Parser parser;
    Opm::ParseContext parseContext;
    const auto deck = parser.parseString(deckString , parseContext);
    std::vector<double> porv;
#endif

    // test PolyhedralGrid
    {
      typedef Dune::PolyhedralGrid< 3, 3 > Grid;
#if HAVE_OPM_PARSER
      Grid grid(deck, porv);
      testGrid( grid, "polyhedralgrid" );
#endif
      //Dune::GridPtr< Grid > gridPtr( dgfFile );
      //testGrid( *gridPtr, "polyhedralgrid-dgf" );
    }

    // test CpGrid
    {
      typedef Dune::CpGrid Grid;
#if HAVE_OPM_PARSER
      Grid grid;
      const int* actnum = deck.hasKeyword("ACTNUM") ? deck.getKeyword("ACTNUM").getIntData().data() : nullptr;
      Opm::EclipseGrid ecl_grid(deck , actnum);

      grid.processEclipseFormat(ecl_grid, false, false, false, porv);
      testGrid( grid, "cpgrid" );

      Opm::UgGridHelpers::createEclipseGrid( grid , ecl_grid );
      testGrid( grid, "cpgrid2" );
#endif
      Dune::GridPtr< Grid > gridPtr( dgfFile );
      //testGrid( *gridPtr, "cpgrid-dgf" );
    }
    return 0;
}
