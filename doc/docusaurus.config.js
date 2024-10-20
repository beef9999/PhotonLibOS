// @ts-check
// Note: type annotations allow type checking and IDEs autocompletion

const lightCodeTheme = require('prism-react-renderer/themes/github');
const darkCodeTheme = require('prism-react-renderer/themes/dracula');

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'PhotonLibOS',
  tagline: 'Probably the fastest coroutine lib of the world',
  favicon: 'img/favicon.ico',

  // Set the production url of your site here
  url: 'https://photonlibos.github.io',
  // Set the /<baseUrl>/ pathname under which your site is served
  // For GitHub pages deployment, it is often '/<projectName>/'
  baseUrl: '/',

  // GitHub pages deployment config.
  // If you aren't using GitHub pages, you don't need these.
  organizationName: 'photonlibos', // Usually your GitHub org/user name.
  projectName: 'photonlibos.github.io', // Usually your repo name.

  trailingSlash: false,       // For better SEO in github
  deploymentBranch: 'main',

  onBrokenLinks: 'throw',
  onBrokenMarkdownLinks: 'warn',

  // Even if you don't use internalization, you can use this field to set useful
  // metadata like html lang. For example, if your site is Chinese, you may want
  // to replace "en" with "zh-Hans".
  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          sidebarPath: require.resolve('./sidebars.js'),
          editUrl: 'https://github.com/alibaba/PhotonLibOS/edit/main/doc/',
        },
        blog: {
          showReadingTime: true,
          editUrl: 'https://github.com/alibaba/PhotonLibOS/edit/main/doc/',
        },
        theme: {
          customCss: require.resolve('./src/css/custom.css'),
        },
      }),
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      // Replace with your project's social card
      image: 'img/social-card.jpg',
      navbar: {
        title: 'PhotonLibOS',
        logo: {
          alt: 'PhotonLibOS Logo',
          src: 'img/logo.svg',
        },
        items: [
          {
            type: 'docSidebar',
            sidebarId: 'docSidebar',
            position: 'left',
            label: 'Docs',
          },
          {
            to: '/blog',
            label: 'Blog', 
            position: 'left',
          },
          {
            href: 'https://github.com/alibaba/PhotonLibOS',
            label: 'GitHub',
            position: 'right',
          },
        ],
      },
      footer: {
        style: 'dark',
        links: [
          {
            title: 'Blog',
            items: [
              {
                label: 'Blog',
                to: '/blog',
              },              
            ],
          },
          {
            title: 'Community',
            items: [
              {
                label: 'Slack',
                href: 'https://photonlibos.slack.com',
              },              
            ],
          },
          {
            title: 'Development',
            items: [             
              {
                label: 'GitHub',
                href: 'https://github.com/alibaba/PhotonLibOS',
              },
            ],
          },
        ],
        copyright: `Copyright © ${new Date().getFullYear()} PhotonLibOS.`,
      },
      prism: {
        theme: lightCodeTheme,
        darkTheme: darkCodeTheme,
        additionalLanguages: ['bash', 'cmake'],
      },
    }),
};

module.exports = config;
