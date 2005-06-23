model:		bench
		designed for use in the game cube
		created 01.2004 using milkshape 3D
           

creator:	dietmar pier, http://www.dietmarpier.de

stats:		vertices: 236
		polygons: 236
		skins: 1

description:	unzip the tris.md2 and skin.pcx files to cube/packages/models/ using 'full path information' (how is this
		option named exactly in the english version of winzip???)

		add the following lines at the END of cube/data/models.cfg

		loadmodel "bench"
		mapmodel 2 2 0 0

		when editing a map, select a floor cube, hit the console key and type 'newent mapmodel XY Z' where XY has to
		be replaced by the correspondent number of the new mapmodel in your models.cfg...

		note: due to the rectangular shape it was nescessary to restrict the model radius to 2. as a result, you will
		be able to walk through the model to the left and right hand. keep this in mind when placing it on a maps...

copyright:	it's free to use for eveyone who likes to. if you want to use it for something else than cube, please drop
		me a line, i just want to know...

credits:	id-software (.tris)

		chumbalum-soft (milkshape3D, it just took 3 days to understand the basics, one more to make my first working
		models for cube)

		aardappel (i love cube! it's so easy/fun to modify things like models, textures, sounds. man, you gave me a
		new chance to make use of my pathologic creativity, since my musical carrier has come to a grinding halt...) 
