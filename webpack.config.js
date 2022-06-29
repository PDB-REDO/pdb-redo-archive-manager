const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const { CleanWebpackPlugin } = require('clean-webpack-plugin');
const webpack = require('webpack');
const path = require('path');

const SCRIPTS = __dirname + "/webapp/";
const SCSS = __dirname + "/scss/";
const DEST = __dirname + "/docroot/";

module.exports = (env) => {

	const webpackConf = {
		entry: {
			index: SCRIPTS + "index.js",
			lists: SCRIPTS + "lists.js",
			search: SCRIPTS + "search.js",

			'pa-style': SCSS + "pdb-redo-bootstrap.scss"
		},

		module: {
			rules: [
				{
					test: /\.js$/,
					exclude: /node_modules/,
					use: {
						loader: "babel-loader",
						options: {
							presets: ['@babel/preset-env']
						}
					}
				},
				{
					test: /\.woff(2)?(\?v=[0-9]\.[0-9]\.[0-9])?$/,
					include: path.resolve(__dirname, './node_modules/bootstrap-icons/font/fonts'),
					type: 'asset/resource',
					generator: {
						filename: 'fonts/[name][ext][query]'
					}
				},

				{
					test: /\.s[ac]ss$/i,
					use: [
						MiniCssExtractPlugin.loader,
						'css-loader',
						'sass-loader'
					]
				},

				{
					test: /\.(png|jpg|gif)$/,
					type: 'asset/resource',
					generator: {
						filename: 'images/[hash][ext][query]'
					}
				}
			]
		},

		output: {
			path: DEST,
			filename: "./scripts/[name].js"
		},

		plugins: [
			new CleanWebpackPlugin({
				cleanOnceBeforeBuildPatterns: [
					'css/**/*',
					'scripts/**/*',
					'fonts/**/*'
				]
			}),
			new webpack.ProvidePlugin({
				$: 'jquery',
				jQuery: 'jquery',
				Popper: ['popper.js', 'default']
			}),
			new MiniCssExtractPlugin({
				filename: './css/[name].css',
				chunkFilename: './css/[id].css'
			})
		]
	};

	const PRODUCTIE = env != null && env.PRODUCTIE;

	if (PRODUCTIE) {
		webpackConf.mode = "production";
		webpackConf.plugins.push(new UglifyJsPlugin({ parallel: 4 }))
	} else {
		webpackConf.mode = "development";
		webpackConf.devtool = 'source-map';
		// webpackConf.plugins.push(new webpack.optimize.AggressiveMergingPlugin())
	}

	return webpackConf;
};
